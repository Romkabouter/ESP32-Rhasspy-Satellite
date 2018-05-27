#include <gtest/gtest.h>

extern "C" {
#include <lwmqtt.h>
#include <lwmqtt/unix.h>
}

#define COMMAND_TIMEOUT 5000

#define PAYLOAD_LEN 256
uint8_t payload[PAYLOAD_LEN + 1];

#define BIG_PAYLOAD_LEN 9800
uint8_t big_payload[BIG_PAYLOAD_LEN + 1];

volatile int counter;

const char *custom_ref = "cool";

static void message_arrived(lwmqtt_client_t *c, void *ref, lwmqtt_string_t t, lwmqtt_message_t m) {
  ASSERT_EQ(ref, custom_ref);

  int res = lwmqtt_strcmp(t, "lwmqtt");
  ASSERT_EQ(res, 0);

  ASSERT_EQ(m.payload_len, (size_t)PAYLOAD_LEN);
  res = memcmp(payload, (char *)m.payload, (size_t)m.payload_len);
  ASSERT_EQ(res, 0);

  counter++;
}

static void big_message_arrived(lwmqtt_client_t *c, void *ref, lwmqtt_string_t t, lwmqtt_message_t m) {
  ASSERT_EQ(ref, custom_ref);

  int res = lwmqtt_strcmp(t, "lwmqtt");
  ASSERT_EQ(res, 0);

  ASSERT_EQ(m.payload_len, (size_t)BIG_PAYLOAD_LEN);
  res = memcmp(big_payload, (char *)m.payload, (size_t)m.payload_len);
  ASSERT_EQ(res, 0);

  counter++;
}

TEST(Client, PublishSubscribeQOS0) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(512), 512, (uint8_t *)malloc(512), 512);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t data = lwmqtt_default_options;
  data.client_id = lwmqtt_string("lwmqtt");
  data.username = lwmqtt_string("try");
  data.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, data, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_subscribe_one(&client, lwmqtt_string("lwmqtt"), LWMQTT_QOS0, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  for (int i = 0; i < 5; i++) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = LWMQTT_QOS0;
    msg.payload = payload;
    msg.payload_len = PAYLOAD_LEN;

    err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
    ASSERT_EQ(err, LWMQTT_SUCCESS);
  }

  while (counter < 5) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_SUCCESS);
    }
  }

  err = lwmqtt_unsubscribe_one(&client, lwmqtt_string("lwmqtt"), COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}

TEST(Client, PublishSubscribeQOS1) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(512), 512, (uint8_t *)malloc(512), 512);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_subscribe_one(&client, lwmqtt_string("lwmqtt"), LWMQTT_QOS1, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  for (int i = 0; i < 5; i++) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = LWMQTT_QOS1;
    msg.payload = payload;
    msg.payload_len = PAYLOAD_LEN;

    err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
    ASSERT_EQ(err, LWMQTT_SUCCESS);
  }

  while (counter < 5) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_SUCCESS);
    }
  }

  err = lwmqtt_unsubscribe_one(&client, lwmqtt_string("lwmqtt"), COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}

TEST(Client, PublishSubscribeQOS2) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(512), 512, (uint8_t *)malloc(512), 512);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_subscribe_one(&client, lwmqtt_string("lwmqtt"), LWMQTT_QOS2, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  for (int i = 0; i < 5; i++) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = LWMQTT_QOS2;
    msg.payload = payload;
    msg.payload_len = PAYLOAD_LEN;

    err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
    ASSERT_EQ(err, LWMQTT_SUCCESS);
  }

  while (counter < 5) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_SUCCESS);
    }
  }

  err = lwmqtt_unsubscribe_one(&client, lwmqtt_string("lwmqtt"), COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}

TEST(Client, BufferOverflowProtection) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(512), 512, (uint8_t *)malloc(512), 256);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_subscribe_one(&client, lwmqtt_string("lwmqtt"), LWMQTT_QOS0, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  lwmqtt_message_t msg = lwmqtt_default_message;
  msg.qos = LWMQTT_QOS0;
  msg.payload = payload;
  msg.payload_len = PAYLOAD_LEN;

  err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  while (counter < 1) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
      break;
    }
  }

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}

TEST(Client, BigBuffersAndPayload) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(10000), 10000, (uint8_t *)malloc(10000), 10000);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, big_message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_subscribe_one(&client, lwmqtt_string("lwmqtt"), LWMQTT_QOS0, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  for (int i = 0; i < 5; i++) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = LWMQTT_QOS0;
    msg.payload = big_payload;
    msg.payload_len = BIG_PAYLOAD_LEN;

    err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
    ASSERT_EQ(err, LWMQTT_SUCCESS);
  }

  while (counter < 5) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_SUCCESS);
    }
  }

  err = lwmqtt_unsubscribe_one(&client, lwmqtt_string("lwmqtt"), COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}

TEST(Client, MultipleSubscriptions) {
  lwmqtt_unix_network_t network;
  lwmqtt_unix_timer_t timer1, timer2;

  lwmqtt_client_t client;

  lwmqtt_init(&client, (uint8_t *)malloc(512), 512, (uint8_t *)malloc(512), 512);

  lwmqtt_set_network(&client, &network, lwmqtt_unix_network_read, lwmqtt_unix_network_write);
  lwmqtt_set_timers(&client, &timer1, &timer2, lwmqtt_unix_timer_set, lwmqtt_unix_timer_get);
  lwmqtt_set_callback(&client, (void *)custom_ref, message_arrived);

  lwmqtt_err_t err = lwmqtt_unix_network_connect(&network, (char *)"broker.shiftr.io", 1883);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_options_t options = lwmqtt_default_options;
  options.client_id = lwmqtt_string("lwmqtt");
  options.username = lwmqtt_string("try");
  options.password = lwmqtt_string("try");

  lwmqtt_return_code_t return_code;
  err = lwmqtt_connect(&client, options, NULL, &return_code, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_string_t topic_filters[2] = {lwmqtt_string("foo"), lwmqtt_string("lwmqtt")};
  lwmqtt_qos_t qos_levels[2] = {LWMQTT_QOS0, LWMQTT_QOS0};

  err = lwmqtt_subscribe(&client, 2, topic_filters, qos_levels, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  counter = 0;

  for (int i = 0; i < 5; i++) {
    lwmqtt_message_t msg = lwmqtt_default_message;
    msg.qos = LWMQTT_QOS0;
    msg.payload = payload;
    msg.payload_len = PAYLOAD_LEN;

    err = lwmqtt_publish(&client, lwmqtt_string("lwmqtt"), msg, COMMAND_TIMEOUT);
    ASSERT_EQ(err, LWMQTT_SUCCESS);
  }

  while (counter < 5) {
    size_t available = 0;
    err = lwmqtt_unix_network_peek(&network, &available);
    ASSERT_EQ(err, LWMQTT_SUCCESS);

    if (available > 0) {
      err = lwmqtt_yield(&client, available, COMMAND_TIMEOUT);
      ASSERT_EQ(err, LWMQTT_SUCCESS);
    }
  }

  err = lwmqtt_unsubscribe(&client, 2, topic_filters, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  err = lwmqtt_disconnect(&client, COMMAND_TIMEOUT);
  ASSERT_EQ(err, LWMQTT_SUCCESS);

  lwmqtt_unix_network_disconnect(&network);
}
