# esp-mqtt

[![Build Status](https://travis-ci.org/256dpi/esp-mqtt.svg?branch=master)](https://travis-ci.org/256dpi/esp-mqtt)
[![Release](https://img.shields.io/github/release/256dpi/esp-mqtt.svg)](https://github.com/256dpi/esp-mqtt/releases)

**MQTT component for esp-idf projects based on the [lwmqtt](https://github.com/256dpi/lwmqtt) library**

This component bundles the lwmqtt client and adds a simple async API similar to other esp networking components.

## Installation

You can install the component by adding it as a git submodule:

```bash
git submodule add https://github.com/256dpi/esp-mqtt.git components/esp-mqtt
git submodule update --init --recursive
```

The component will automatically enable the LWIP receive buffers. 

## API

Initialize the component once by passing the necessary callbacks:

```c++
void esp_mqtt_init(esp_mqtt_status_callback_t scb, esp_mqtt_message_callback_t mcb,
                   size_t buffer_size, int command_timeout);
```

When the WiFi connection has been established, start the process:

```c++
void esp_mqtt_start(const char *host, int port, const char *client_id,
                    const char *username, const char *password);
```

When the client has connected, interact with the broker:

```c++
bool esp_mqtt_subscribe(const char *topic, int qos);
bool esp_mqtt_unsubscribe(const char *topic);
bool esp_mqtt_publish(const char *topic, uint8_t *payload, size_t len, int qos, bool retained);
```

If the WiFi connection has been lost, stop the process:

```c++
void esp_mqtt_stop();
```
