#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <lwmqtt/unix.h>

#define COMMAND_TIMEOUT 5000
#define MESSAGE_TIMEOUT 1000

lwmqtt_unix_network_t network = {0};

lwmqtt_unix_timer_t timer1, timer2, timer3;

lwmqtt_client_t client;

static void message_arrived(lwmqtt_client_t *client, void *ref, lwmqtt_string_t topic, lwmqtt_message_t msg) {
  printf("message_arrived: %.*s => %.*s (%d)\n", (int)topic.len, topic.data, (int)msg.payload_len, (char *)msg.payload,
         (int)msg.payload_len);
}

int main() {
  // initialize client
  lwmqtt_init(&client, malloc(512), 512, malloc(512), 512);

  // configure client
  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, NULL, message_arrived);

  // configure message time
  lwmqtt_unix_timer_set(&timer3, MESSAGE_TIMEOUT);

  // connect to broker
  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, "broker.shiftr.io", 1883);
  if (err != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_unix_network_connect: %d\n", err);
    exit(1);
  }

  // prepare options
  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");
  options.keep_alive = 5;

  // send connect packet
  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  if (err != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_connect: %d (%d)\n", err, return_code);
    exit(1);
  }

  // log
  printf("connected!\n");

  // subscribe to topic
  err = lwmqtt_subscribe_one(&client, lwmqtt_string("hello"), LWMQTT_QOS0, COMMAND_TIMEOUT);
  if (err != LWMQTT_SUCCESS) {
    printf("failed lwmqtt_subscribe: %d\n", err);
    exit(1);
  }

  // loop forever
  for (;;) {
    // check if data is available
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    if (err != LWMQTT_SUCCESS) {
      printf("failed lwmqtt_unix_network_peek: %d\n", err);
      exit(1);
    }

    // process data if available
    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      if (err != LWMQTT_SUCCESS) {
        printf("failed lwmqtt_yield: %d\n", err);
        exit(1);
      }
    }

    // keep connection alive
    err = lwmqtt_keep_alive(&client, COMMAND_TIMEOUT);
    if (err != LWMQTT_SUCCESS) {
      printf("failed lwmqtt_keep_alive: %d\n", err);
      exit(1);
    }

    // check if message is due
    if (lwmqtt_unix_timer_get(&timer3) <= 0) {
      // prepare message
      lwmqtt_message_t msg = {.qos = LWMQTT_QOS0, .retained = false, .payload = (uint8_t *)("world"), .payload_len = 5};

      // publish message
      err = lwmqtt_publish(&client, lwmqtt_string("hello"), msg, COMMAND_TIMEOUT);
      if (err != LWMQTT_SUCCESS) {
        printf("failed lwmqtt_keep_alive: %d\n", err);
        exit(1);
      }

      // reset timer
      lwmqtt_unix_timer_set(&timer3, MESSAGE_TIMEOUT);
    }

    // sleep for 100ms
    usleep(100 * 1000);
  }
}
