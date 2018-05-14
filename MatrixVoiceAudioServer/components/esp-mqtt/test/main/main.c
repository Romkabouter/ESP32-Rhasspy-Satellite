#include <stdlib.h>
#include <string.h>

#include <esp_event_loop.h>
#include <esp_mqtt.h>
#include <esp_wifi.h>

#define WIFI_SSID "ssid"
#define WIFI_PASS "pass"

#define MQTT_HOST "broker.shiftr.io"
#define MQTT_PORT 1883
#define MQTT_USER "try"
#define MQTT_PASS "try"

static TaskHandle_t task = NULL;

static void process(void *p) {
  for (;;) {
    esp_mqtt_publish("hello", (uint8_t *)"world", 5, 0, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
  switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
      esp_wifi_connect();
      break;

    case SYSTEM_EVENT_STA_GOT_IP:
      esp_mqtt_start(MQTT_HOST, MQTT_PORT, "esp-mqtt", MQTT_USER, MQTT_PASS);
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      esp_mqtt_stop();
      esp_wifi_connect();
      break;

    default:
      break;
  }

  return ESP_OK;
}

static void status_callback(esp_mqtt_status_t status) {
  switch (status) {
    case ESP_MQTT_STATUS_CONNECTED:
      esp_mqtt_subscribe("hello", 0);
      xTaskCreatePinnedToCore(process, "process", 1024, NULL, 10, &task, 1);
      break;
    case ESP_MQTT_STATUS_DISCONNECTED:
      vTaskDelete(task);
      break;
  }
}

static void message_callback(const char *topic, uint8_t *payload, size_t len) {
  printf("incoming: %s => %s (%d)\n", topic, payload, (int)len);
}

void app_main() {
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
          },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_mqtt_init(status_callback, message_callback, 256, 2000);
}
