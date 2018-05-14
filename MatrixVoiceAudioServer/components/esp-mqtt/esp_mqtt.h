#ifndef ESP_MQTT_H
#define ESP_MQTT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * The statuses emitted by the callback.
 */
typedef enum esp_mqtt_status_t { ESP_MQTT_STATUS_DISCONNECTED, ESP_MQTT_STATUS_CONNECTED } esp_mqtt_status_t;

/**
 * The status change callback.
 */
typedef void (*esp_mqtt_status_callback_t)(esp_mqtt_status_t);

/**
 * The message callback.
 */
typedef void (*esp_mqtt_message_callback_t)(const char *topic, uint8_t *payload, size_t len);

/**
 * Initialize the MQTT management system.
 *
 * Note: Should only be called once on boot.
 *
 * @param scb - The status callback.
 * @param mcb - The message callback.
 * @param buffer_size - The read and write buffer size.
 * @param command_timeout - The command timeout.
 */
void esp_mqtt_init(esp_mqtt_status_callback_t scb, esp_mqtt_message_callback_t mcb, size_t buffer_size,
                   int command_timeout);

/**
 * Start the MQTT process.
 *
 * @param host - The broker host.
 * @param port - The broker port.
 * @param client_id - The client id.
 * @param username - The client username.
 * @param password - The client password.
 */
void esp_mqtt_start(const char *host, int port, const char *client_id, const char *username, const char *password);

/**
 * Subscribe to specified topic.
 *
 * @param topic - The topic.
 * @param qos - The qos level.
 * @return
 */
bool esp_mqtt_subscribe(const char *topic, int qos);

/**
 * Unsubscribe from specified topic.
 *
 * @param topic - The topic.
 * @return
 */
bool esp_mqtt_unsubscribe(const char *topic);

/**
 * Publish bytes payload to specified topic.
 *
 * @param topic - The topic.
 * @param payload - The payload.
 * @param len - The payload length.
 * @param qos - The qos level.
 * @param retained - The retained flag.
 * @return
 */
bool esp_mqtt_publish(const char *topic, uint8_t *payload, size_t len, int qos, bool retained);

/**
 * Stop the MQTT process.
 */
void esp_mqtt_stop();

#endif  // ESP_MQTT_H
