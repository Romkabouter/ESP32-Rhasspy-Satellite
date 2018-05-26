//
//  network.c
//  esp32-ota
//
//  Networking code for OTA demo application
//
//  Created by Andreas Schweizer on 02.12.2016.
//  Copyright © 2016 Classy Code GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_mqtt.h"
#include "network.h"
#include "ota_update.h"


typedef enum {
    NWR_READ_COMPLETE = 0,
    NWR_READ_TIMEOUT = 1,
    NWR_DISCONNECTED = 2,
    NWR_ERROR = 3,
} TNetworkResult;

#define TAG "network"

#define NORM_C(c) (((c) >= 32 && (c) < 127) ? (c) : '.')
#define MQTT_HOST CONFIG_MQTT_HOST
#define MQTT_PORT CONFIG_MQTT_PORT
#define MQTT_USER CONFIG_MQTT_USER
#define MQTT_PASS CONFIG_MQTT_PASS
#define SITEID CONFIG_SITEID
#define BUFFER_SIZE 1000 //be sure to create enough buffer
#define AP_CONNECTION_ESTABLISHED (1 << 0)
#define MQTT_CONNECTION_ESTABLISHED (1 << 0)
#define HOTWORD_DETECTED (1 << 0)
static EventGroupHandle_t sEventGroup;
static EventGroupHandle_t mqttEventGroup;
static EventGroupHandle_t hwEventGroup;

// Indicates that we should trigger a re-boot after sending the response.
static int sRebootAfterReply;


// Listen to TCP requests on port 80
static void networkTask(void *pvParameters);

static void processMessage(const char *message, int messageLen, char *responseBuf, int responseBufLen);
static int networkReceive(int s, char *buf, int maxLen, int *actualLen);
static void networkSetConnected(uint8_t c);
static esp_err_t eventHandler(void *ctx, system_event_t *event);
static void mqtt_status_cb(esp_mqtt_status_t status);
static void mqtt_message_cb(const char *topic, uint8_t *payload, size_t len);

void networkInit()
{
    ESP_LOGI(TAG, "networkInit");
    
    ESP_ERROR_CHECK( esp_event_loop_init(eventHandler, NULL) );
    tcpip_adapter_init();

    sEventGroup = xEventGroupCreate();
    mqttEventGroup = xEventGroupCreate();
    hwEventGroup = xEventGroupCreate();
    xTaskCreate(&networkTask, "networkTask", 32768, NULL, 5, NULL);
    
    otaUpdateInit();
    //
    esp_mqtt_init(mqtt_status_cb, mqtt_message_cb, BUFFER_SIZE, 2000);
}

void networkConnect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "networkConnect %s", ssid);
    
    wifi_init_config_t wlanInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&wlanInitConfig) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wlanStaConfig = {};
    strcpy((char *)wlanStaConfig.sta.ssid, ssid);
    strcpy((char *)wlanStaConfig.sta.password, password);

    wlanStaConfig.sta.bssid_set = false;
    
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wlanStaConfig) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

int networkIsConnected()
{
    return xEventGroupGetBits(sEventGroup) & AP_CONNECTION_ESTABLISHED;
}

int HotwordDetected()
{
    return xEventGroupGetBits(hwEventGroup) & HOTWORD_DETECTED;
}

int mqttIsConnected()
{
    return xEventGroupGetBits(mqttEventGroup) & MQTT_CONNECTION_ESTABLISHED;
}

static void networkTask(void *pvParameters)
{
    const int maxRequestLen = 10000;
    const int maxResponseLen = 1000;
    const int tcpPort = 80;
    
    ESP_LOGI(TAG, "networkTask");
    
    while (1) {

        // Barrier for the connection (we need to be connected to an AP).
        xEventGroupWaitBits(sEventGroup, AP_CONNECTION_ESTABLISHED, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "networkTask: connected to access point");
        
        
        // Create TCP socket.
        
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) {
            ESP_LOGE(TAG, "networkTask: failed to create socket: %d (%s)", errno, strerror(errno));
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        
        
        // Bind socket to port.
        
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof (struct sockaddr_in));
        serverAddr.sin_len = sizeof(struct sockaddr_in);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(tcpPort);
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        
        int b = bind(s, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
        if (b < 0) {
            ESP_LOGE(TAG, "networkTask: failed to bind socket %d: %d (%s)", s, errno, strerror(errno));
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        
        
        // Listen to incoming connections.
        
        ESP_LOGD(TAG, "networkTask: 'listen' on socket %d", s);
        listen(s, 1); // backlog max. 1 connection
        while (1) {
            
            
            // Accept the connection on a separate socket.
            
            ESP_LOGD(TAG, "--------------------");
            ESP_LOGD(TAG, "networkTask: 'accept' on socket %d", s);
            struct sockaddr_in clientAddr;
            socklen_t clen = sizeof(clientAddr);
            int s2 = accept(s, (struct sockaddr *)&clientAddr, &clen);
            if (s2 < 0) {
                ESP_LOGE(TAG, "networkTask: 'accept' failed: %d (%s)", errno, strerror(errno));
                vTaskDelay(1000 / portTICK_RATE_MS);
                break;
            }
            
            // Would normally fork here.
            // For the moment, we support only a single open connection at any time.
            
            do {
                //ESP_LOGD(TAG, "networkTask: waiting for data on socket %d...", s2);


                // Allocate and clear memory for the request data.
                
                char *requestBuf = malloc(maxRequestLen * sizeof(char));
                if (!requestBuf) {
                    ESP_LOGE(TAG, "networkTask: malloc for requestBuf failed: %d (%s)", errno, strerror(errno));
                    break;
                }
                bzero(requestBuf, maxRequestLen);
                
                
                // Read the request and store it in the allocated buffer.
                
                int totalRequestLen = 0;
                TNetworkResult result = networkReceive(s2, requestBuf, maxRequestLen, &totalRequestLen);
                
                if (result != NWR_READ_COMPLETE) {
                    ESP_LOGI(TAG, "nothing more to, closing socket %d", s2);
                    free(requestBuf);
                    close(s2);
                    break;
                }
            
                // Read completed successfully.
                // Process the request and create the response.
            
                ESP_LOGI(TAG, "networkTask: received %d bytes: %02x %02x %02x %02x ... | %c%c%c%c...",
                         totalRequestLen,
                         requestBuf[0], requestBuf[1], requestBuf[2], requestBuf[3],
                         NORM_C(requestBuf[0]), NORM_C(requestBuf[1]), NORM_C(requestBuf[2]), NORM_C(requestBuf[3]));
            
            
                char *responseBuf = malloc(maxResponseLen * sizeof(char));
                processMessage(requestBuf, totalRequestLen, responseBuf, maxResponseLen);

                free(requestBuf);
                
                
                // Send the response back to the client.
            
                int totalLen = strlen(responseBuf);
                int nofWritten = 0;
                ESP_LOGD(TAG, "networkTask: write %d bytes to socket %d: %02x %02x %02x %02x ... | %c%c%c%c...", totalLen, s2,
                         responseBuf[0], responseBuf[1], responseBuf[2], responseBuf[3],
                         NORM_C(responseBuf[0]), NORM_C(responseBuf[1]), NORM_C(responseBuf[2]), NORM_C(responseBuf[3]));

                do {
                    int n = write(s2, &responseBuf[nofWritten], totalLen - nofWritten);
                    int e = errno;
                    //ESP_LOGD(TAG, "networkTask: write: socket %d, n = %d, errno = %d", s2, n, e);
                    
                    if (n > 0) {
                        nofWritten += n;
                        
                        // More to write?
                        if (totalLen - nofWritten == 0) {
                            break;
                        }
                        
                    } else if (n == 0) {
                        // Disconnected?
                        break;
                        
                    } else {
                        // n < 0
                        if (e == EAGAIN) {
                            //ESP_LOGD(TAG, "networkTask: write: EAGAIN");
                            continue;
                        }
                        ESP_LOGE(TAG, "networkTask: write failed: %d (%s)", errno, strerror(errno));
                        break;
                    }
                    
                } while (1);
                
                free(responseBuf);
                
                
                if (sRebootAfterReply) {
                    ESP_LOGI(TAG, "networkTask: Reboot in 2 seconds...");
                    vTaskDelay(2000 / portTICK_RATE_MS);
                    esp_restart();
                }

            } while (1);
        }
        
        // Should never arrive here
        close(s);
        vTaskDelay(2000 / portTICK_RATE_MS);
    }
}

static int networkReceive(int s, char *buf, int maxLen, int *actualLen)
{
    //ESP_LOGI(TAG, "networkReceive: start maxlen = %d", maxLen);
    
    int totalLen = 0;
    
    for (int timeoutCtr = 0; timeoutCtr < 3000; timeoutCtr++) {

        int readAgain = 0;
        do {
            buf[totalLen] = 0x00;
            int n = recv(s, &buf[totalLen], maxLen - totalLen, MSG_DONTWAIT);
            int e = errno;

            // Error?
            if (n > 0) {
                // Message complete?
                totalLen += n;
                if (totalLen > 0) {
                    // We currently support two record types:
                    // Records that start with !xxxx where x is a hexadecimal length indicator
                    // Records that start with something else and are terminated with a newline
                    int recordLen = 0;
                    int recordWithLengthIndicator = (1 == sscanf(buf, "!%04x", &recordLen));
                    ESP_LOGD(TAG, "networkReceive: recordWithLengthIndicator = %d, expected length = %d, current length = %d", recordWithLengthIndicator, recordLen, totalLen);
                    if ((recordWithLengthIndicator && totalLen == recordLen)
                        || (!recordWithLengthIndicator && buf[totalLen - 1] == '\n'))
                    {
                        ESP_LOGI(TAG, "networkReceive: received %d byte packet on socket %d", totalLen, s);
                        *actualLen = totalLen;
                        return NWR_READ_COMPLETE;
                    }
                }
                // Not yet complete. Read again immediately.
                readAgain = 1;
                
            } else if (n < 0 && e == EAGAIN) {
                // No data available right now.
                // Wait for a short moment before trying again.
                readAgain = 0;

            } else {
                // Error (n=0, n<0).
                ESP_LOGE(TAG, "recv n = %d, errno = %d (%s)", n, e, strerror(e));
                return NWR_ERROR;
            }
            
        } while (readAgain);
        
        // n == 0, wait a bit
        //ESP_LOGI(TAG, "networkReceive: wait for more data");
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    
    return NWR_READ_TIMEOUT;
}

static void processMessage(const char *message, int messageLen, char *responseBuf, int responseBufLen)
{
    // Response to send back to the TCP client.
    char response[256];
    sprintf(response, "OK\r\n");
    
    if (message[0] == '!') {
        TOtaResult result = OTA_OK;
        
        if (message[1] == '[') {
            ESP_LOGI(TAG, "processMessage: OTA start");
            result = otaUpdateBegin();
            
        } else if (message[1] == ']') {
            ESP_LOGI(TAG, "processMessage: OTA end");
            result = otaUpdateEnd();
            
        } else if (message[1] == '*') {
            ESP_LOGI(TAG, "processMessage: Reboot");
            sRebootAfterReply = 1;
            
        } else {
            result = otaUpdateWriteHexData(&message[5], messageLen - 5);
        }

        if (result != OTA_OK) {
            ESP_LOGE(TAG, "processMessage: OTA_ERROR %d", result);
            sprintf(response, "OTA_ERROR %d\r\n", result);
        }
        
    } else if (message[0] == '?') {
        otaDumpInformation();
    }
    
    strncpy(responseBuf, response, responseBufLen);
}

static void networkSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(sEventGroup, AP_CONNECTION_ESTABLISHED);
    }
}

static esp_err_t eventHandler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
            
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_START");
            esp_wifi_connect();
            break;
            
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_GOT_IP");
            esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
            networkSetConnected(1);
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_STA_DISCONNECTED");
            esp_mqtt_stop();
            // try to re-connect
            esp_wifi_connect();
            networkSetConnected(0);
            break;
        
        case SYSTEM_EVENT_AP_START:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_START");
            break;
        
        case SYSTEM_EVENT_AP_STOP:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED");
            break;
            
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STACONNECTED: " MACSTR " id=%d",
                     MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
            break;
        
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED: " MACSTR " id=%d",
                     MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
            break;
        
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            ESP_LOGI(TAG, "eventHandler: SYSTEM_EVENT_AP_STADISCONNECTED: " MACSTR " rssi=%d",
                     MAC2STR(event->event_info.ap_probereqrecved.mac), event->event_info.ap_probereqrecved.rssi);
            break;
            
        default:
            break;
    }
    
    return ESP_OK;
}

static void MqttSetConnected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(mqttEventGroup, MQTT_CONNECTION_ESTABLISHED);
    } else {
        xEventGroupClearBits(mqttEventGroup, MQTT_CONNECTION_ESTABLISHED);
    }
}

static void HotWordSetDetected(uint8_t c)
{
    if (c) {
        xEventGroupSetBits(hwEventGroup, HOTWORD_DETECTED);
    } else {
        xEventGroupClearBits(hwEventGroup, HOTWORD_DETECTED);
    }
}

static void mqtt_status_cb(esp_mqtt_status_t status) {
    switch (status)
    {
    case ESP_MQTT_STATUS_CONNECTED:
        //set connected to true to be able to send packets
        esp_mqtt_subscribe("hermes/hotword/toggleOff", 0);
        esp_mqtt_subscribe("hermes/hotword/toggleOn", 0);
        MqttSetConnected(1);
        break;
    case ESP_MQTT_STATUS_DISCONNECTED:
        //Not connected anymore, stop trying to send
        MqttSetConnected(0);
        break;
    }
}

static void mqtt_message_cb(const char *topic, uint8_t *payload, size_t len) {
    const cJSON *site = NULL;
    if ((int)strstr (topic, "toggleOff") > 0) {
        cJSON *json = cJSON_Parse((const char *)payload);
        site = cJSON_GetObjectItemCaseSensitive(json, "siteId");
        if (strcmp(site->valuestring,SITEID) == 0) {
          HotWordSetDetected(1);
        }    
        cJSON_Delete(json);
    } else if ((int)strstr (topic, "toggleOn") > 0) {
        cJSON *json = cJSON_Parse((const char *)payload);
        site = cJSON_GetObjectItemCaseSensitive(json, "siteId");
        if (strcmp(site->valuestring,SITEID) == 0) {
         HotWordSetDetected(0);
        }
        cJSON_Delete(json);
    }
}
