#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lwmqtt.h>
#include <string.h>

#include "esp_lwmqtt.h"
#include "esp_mqtt.h"

#define ESP_MQTT_LOG_TAG "esp_mqtt"

static SemaphoreHandle_t esp_mqtt_mutex = NULL;

#define ESP_MQTT_LOCK() \
  do {                  \
  } while (xSemaphoreTake(esp_mqtt_mutex, portMAX_DELAY) != pdPASS)

#define ESP_MQTT_UNLOCK() xSemaphoreGive(esp_mqtt_mutex)

static TaskHandle_t esp_mqtt_task = NULL;

static size_t esp_mqtt_buffer_size;
static uint32_t esp_mqtt_command_timeout;

static struct {
  char *host;
  int port;
  char *client_id;
  char *username;
  char *password;
} esp_mqtt_config = {.host = NULL, .port = 1883, .client_id = NULL, .username = NULL, .password = NULL};

static bool esp_mqtt_running = false;
static bool esp_mqtt_connected = false;

static esp_mqtt_status_callback_t esp_mqtt_status_callback = NULL;
static esp_mqtt_message_callback_t esp_mqtt_message_callback = NULL;

static lwmqtt_client_t esp_mqtt_client;

static esp_lwmqtt_network_t esp_mqtt_network = {0};

static esp_lwmqtt_timer_t esp_mqtt_timer1, esp_mqtt_timer2;

static void *esp_mqtt_write_buffer;
static void *esp_mqtt_read_buffer;

static QueueHandle_t esp_mqtt_event_queue = NULL;

typedef struct {
  lwmqtt_string_t topic;
  lwmqtt_message_t message;
} esp_mqtt_event_t;

void esp_mqtt_init(esp_mqtt_status_callback_t scb, esp_mqtt_message_callback_t mcb, size_t buffer_size,
                   int command_timeout) {
  // set callbacks
  esp_mqtt_status_callback = scb;
  esp_mqtt_message_callback = mcb;
  esp_mqtt_buffer_size = buffer_size;
  esp_mqtt_command_timeout = (uint32_t)command_timeout;

  // allocate buffers
  esp_mqtt_write_buffer = malloc((size_t)buffer_size);
  esp_mqtt_read_buffer = malloc((size_t)buffer_size);

  // create mutex
  esp_mqtt_mutex = xSemaphoreCreateMutex();

  // create queue
  esp_mqtt_event_queue = xQueueCreate(CONFIG_ESP_MQTT_EVENT_QUEUE_SIZE, sizeof(esp_mqtt_event_t *));
}

static void esp_mqtt_message_handler(lwmqtt_client_t *client, void *ref, lwmqtt_string_t topic, lwmqtt_message_t msg) {
  // create message
  esp_mqtt_event_t *evt = malloc(sizeof(esp_mqtt_event_t));

  // copy topic with additional null termination
  evt->topic.len = topic.len;
  evt->topic.data = malloc((size_t)topic.len + 1);
  memcpy(evt->topic.data, topic.data, (size_t)topic.len);
  evt->topic.data[topic.len] = 0;

  // copy message with additional null termination
  evt->message.retained = msg.retained;
  evt->message.qos = msg.qos;
  evt->message.payload_len = msg.payload_len;
  evt->message.payload = malloc((size_t)msg.payload_len + 1);
  memcpy(evt->message.payload, msg.payload, (size_t)msg.payload_len);
  evt->message.payload[msg.payload_len] = 0;

  // queue event
  if (xQueueSend(esp_mqtt_event_queue, &evt, 0) != pdTRUE) {
    ESP_LOGW(ESP_MQTT_LOG_TAG, "xQueueSend: queue is full, dropping message");
    free(evt->topic.data);
    free(evt->message.payload);
    free(evt);
  }
}

static void esp_mqtt_dispatch_events() {
  // prepare event
  esp_mqtt_event_t *evt = NULL;

  // receive next event
  while (xQueueReceive(esp_mqtt_event_queue, &evt, 0) == pdTRUE) {
    // call callback if existing
    if (esp_mqtt_message_callback) {
      esp_mqtt_message_callback(evt->topic.data, evt->message.payload, evt->message.payload_len);
    }

    // free data
    free(evt->topic.data);
    free(evt->message.payload);
    free(evt);
  }
}

static bool esp_mqtt_process_connect() {
  // initialize the client
  lwmqtt_init(&esp_mqtt_client, esp_mqtt_write_buffer, esp_mqtt_buffer_size, esp_mqtt_read_buffer,
              esp_mqtt_buffer_size);
  lwmqtt_set_network(&esp_mqtt_client, &esp_mqtt_network, esp_lwmqtt_network_read, esp_lwmqtt_network_write);
  lwmqtt_set_timers(&esp_mqtt_client, &esp_mqtt_timer1, &esp_mqtt_timer2, esp_lwmqtt_timer_set, esp_lwmqtt_timer_get);
  lwmqtt_set_callback(&esp_mqtt_client, NULL, esp_mqtt_message_handler);

  // attempt network connection
  lwmqtt_err_t err = esp_lwmqtt_network_connect(&esp_mqtt_network, esp_mqtt_config.host, esp_mqtt_config.port);
  if (err != LWMQTT_SUCCESS) {
    ESP_LOGE(ESP_MQTT_LOG_TAG, "esp_lwmqtt_network_connect: %d", err);
    return false;
  }

  // setup connect data
  lwmqtt_options_t options = lwmqtt_default_options;
  options.keep_alive = 10;
  options.client_id = lwmqtt_string(esp_mqtt_config.client_id);
  options.username = lwmqtt_string(esp_mqtt_config.username);
  options.password = lwmqtt_string(esp_mqtt_config.password);

  // attempt connection
  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&esp_mqtt_client, options, NULL, &return_code, esp_mqtt_command_timeout);
  if (err != LWMQTT_SUCCESS) {
    ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_connect: %d", err);
    return false;
  }

  return true;
}

static void esp_mqtt_process(void *p) {
  // connection loop
  for (;;) {
    // log attempt
    ESP_LOGI(ESP_MQTT_LOG_TAG, "esp_mqtt_process: begin connection attempt");

    // acquire mutex
    ESP_MQTT_LOCK();

    // make connection attempt
    if (esp_mqtt_process_connect()) {
      // log success
      ESP_LOGI(ESP_MQTT_LOG_TAG, "esp_mqtt_process: connection attempt successful");

      // set local flag
      esp_mqtt_connected = true;

      // release mutex
      ESP_MQTT_UNLOCK();

      // exit loop
      break;
    }

    // release mutex
    ESP_MQTT_UNLOCK();

    // log fail
    ESP_LOGW(ESP_MQTT_LOG_TAG, "esp_mqtt_process: connection attempt failed");

    // delay loop by 1s and yield to other processes
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  // call callback if existing
  if (esp_mqtt_status_callback) {
    esp_mqtt_status_callback(ESP_MQTT_STATUS_CONNECTED);
  }

  // yield loop
  for (;;) {
    // dispatch queued events
    esp_mqtt_dispatch_events();

    // acquire mutex
    ESP_MQTT_LOCK();

    // get the available bytes to be read
    size_t available = 0;
    lwmqtt_err_t err = esp_lwmqtt_network_peek(&esp_mqtt_network, &available);
    if (err != LWMQTT_SUCCESS) {
      ESP_LOGE(ESP_MQTT_LOG_TAG, "esp_lwmqtt_network_peek: %d", err);
      break;
    }

    // yield to client if data is available
    if (available > 0) {
      err = lwmqtt_yield(&esp_mqtt_client, available, esp_mqtt_command_timeout);
      if (err != LWMQTT_SUCCESS) {
        ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_yield: %d", err);
        break;
      }
    }

    // do mqtt background work
    err = lwmqtt_keep_alive(&esp_mqtt_client, esp_mqtt_command_timeout);
    if (err != LWMQTT_SUCCESS) {
      ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_keep_alive: %d", err);
      break;
    }

    // release mutex
    ESP_MQTT_UNLOCK();

    // dispatch queued events
    esp_mqtt_dispatch_events();

    // yield to other processes
    vTaskDelay(1);
  }

  // mutex has already been acquired above

  // disconnect network
  esp_lwmqtt_network_disconnect(&esp_mqtt_network);

  // set local flags
  esp_mqtt_connected = false;
  esp_mqtt_running = false;

  // release mutex
  ESP_MQTT_UNLOCK();

  ESP_LOGI(ESP_MQTT_LOG_TAG, "esp_mqtt_process: exit task");

  // call callback if existing
  if (esp_mqtt_status_callback) {
    esp_mqtt_status_callback(ESP_MQTT_STATUS_DISCONNECTED);
  }

  // delete task
  vTaskDelete(NULL);
}

void esp_mqtt_start(const char *host, int port, const char *client_id, const char *username, const char *password) {
  // acquire mutex
  ESP_MQTT_LOCK();

  // check if already running
  if (esp_mqtt_running) {
    ESP_LOGW(ESP_MQTT_LOG_TAG, "esp_mqtt_start: already running");
    ESP_MQTT_UNLOCK();
    return;
  }

  // free host if set
  if (esp_mqtt_config.host != NULL) {
    free(esp_mqtt_config.host);
    esp_mqtt_config.host = NULL;
  }

  // free client id if set
  if (esp_mqtt_config.client_id != NULL) {
    free(esp_mqtt_config.client_id);
    esp_mqtt_config.client_id = NULL;
  }

  // free username if set
  if (esp_mqtt_config.username != NULL) {
    free(esp_mqtt_config.username);
    esp_mqtt_config.username = NULL;
  }

  // free password if set
  if (esp_mqtt_config.password != NULL) {
    free(esp_mqtt_config.password);
    esp_mqtt_config.password = NULL;
  }

  // set host if provided
  if (host != NULL) {
    esp_mqtt_config.host = strdup(host);
  }

  // set port
  esp_mqtt_config.port = port;

  // set client id if provided
  if (client_id != NULL) {
    esp_mqtt_config.client_id = strdup(client_id);
  }

  // set username if provided
  if (username != NULL) {
    esp_mqtt_config.username = strdup(username);
  }

  // set password if provided
  if (password != NULL) {
    esp_mqtt_config.password = strdup(password);
  }

  // create mqtt thread
  ESP_LOGI(ESP_MQTT_LOG_TAG, "esp_mqtt_start: create task");
  xTaskCreatePinnedToCore(esp_mqtt_process, "esp_mqtt", CONFIG_ESP_MQTT_TASK_STACK_SIZE, NULL,
                          CONFIG_ESP_MQTT_TASK_STACK_PRIORITY, &esp_mqtt_task, 1);

  // set local flag
  esp_mqtt_running = true;

  // release mutex
  ESP_MQTT_UNLOCK();
}

bool esp_mqtt_subscribe(const char *topic, int qos) {
  // acquire mutex
  ESP_MQTT_LOCK();

  // check if still connected
  if (!esp_mqtt_connected) {
    ESP_LOGW(ESP_MQTT_LOG_TAG, "esp_mqtt_subscribe: not connected");
    ESP_MQTT_UNLOCK();
    return false;
  }

  // subscribe to topic
  lwmqtt_err_t err =
      lwmqtt_subscribe_one(&esp_mqtt_client, lwmqtt_string(topic), (lwmqtt_qos_t)qos, esp_mqtt_command_timeout);
  if (err != LWMQTT_SUCCESS) {
    ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_subscribe_one: %d", err);
    ESP_MQTT_UNLOCK();
    return false;
  }

  // release mutex
  ESP_MQTT_UNLOCK();

  return true;
}

bool esp_mqtt_unsubscribe(const char *topic) {
  // acquire mutex
  ESP_MQTT_LOCK();

  // check if still connected
  if (!esp_mqtt_connected) {
    ESP_LOGW(ESP_MQTT_LOG_TAG, "esp_mqtt_unsubscribe: not connected");
    ESP_MQTT_UNLOCK();
    return false;
  }

  // unsubscribe from topic
  lwmqtt_err_t err = lwmqtt_unsubscribe_one(&esp_mqtt_client, lwmqtt_string(topic), esp_mqtt_command_timeout);
  if (err != LWMQTT_SUCCESS) {
    ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_unsubscribe_one: %d", err);
    ESP_MQTT_UNLOCK();
    return false;
  }

  // release mutex
  ESP_MQTT_UNLOCK();

  return true;
}

bool esp_mqtt_publish(const char *topic, uint8_t *payload, size_t len, int qos, bool retained) {
  // acquire mutex
  ESP_MQTT_LOCK();

  // check if still connected
  if (!esp_mqtt_connected) {
    ESP_LOGW(ESP_MQTT_LOG_TAG, "esp_mqtt_publish: not connected");
    ESP_MQTT_UNLOCK();
    return false;
  }

  // prepare message
  lwmqtt_message_t message;
  message.qos = (lwmqtt_qos_t)qos;
  message.retained = retained;
  message.payload = payload;
  message.payload_len = len;

  // publish message
  lwmqtt_err_t err = lwmqtt_publish(&esp_mqtt_client, lwmqtt_string(topic), message, esp_mqtt_command_timeout);
  if (err != LWMQTT_SUCCESS) {
    ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_publish: %d", err);
    ESP_MQTT_UNLOCK();
    return false;
  }

  // release mutex
  ESP_MQTT_UNLOCK();

  return true;
}

void esp_mqtt_stop() {
  // acquire mutex
  ESP_MQTT_LOCK();

  // return immediately if not running anymore
  if (!esp_mqtt_running) {
    ESP_MQTT_UNLOCK();
    return;
  }

  // attempt to properly disconnect a connected client
  if (esp_mqtt_connected) {
    lwmqtt_err_t err = lwmqtt_disconnect(&esp_mqtt_client, esp_mqtt_command_timeout);
    if (err != LWMQTT_SUCCESS) {
      ESP_LOGE(ESP_MQTT_LOG_TAG, "lwmqtt_disconnect: %d", err);
    }

    // set flag
    esp_mqtt_connected = false;
  }

  // disconnect network
  esp_lwmqtt_network_disconnect(&esp_mqtt_network);

  // kill mqtt task
  ESP_LOGI(ESP_MQTT_LOG_TAG, "esp_mqtt_stop: deleting task");
  vTaskDelete(esp_mqtt_task);

  // set flag
  esp_mqtt_running = false;

  // release mutex
  ESP_MQTT_UNLOCK();
}
