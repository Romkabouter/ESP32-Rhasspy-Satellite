#include <gtest/gtest.h>

extern "C" {
#include <lwmqtt.h>
#include "../src/packet.h"
}

#define EXPECT_ARRAY_EQ(reference, actual, element_count)                 \
  {                                                                       \
    for (size_t cmp_i = 0; cmp_i < element_count; cmp_i++) {              \
      EXPECT_EQ(reference[cmp_i], actual[cmp_i]) << "At byte: " << cmp_i; \
    }                                                                     \
  }

TEST(DetectPacketType, Valid) {
  uint8_t h = LWMQTT_CONNACK_PACKET << 4;
  lwmqtt_packet_type_t p;
  lwmqtt_err_t err = lwmqtt_detect_packet_type(&h, 1, &p);
  EXPECT_EQ(p, LWMQTT_CONNACK_PACKET);
  EXPECT_EQ(err, LWMQTT_SUCCESS);
}

TEST(DetectPacketType, Invalid) {
  uint8_t h = 255;
  lwmqtt_packet_type_t p;
  lwmqtt_err_t err = lwmqtt_detect_packet_type(&h, 1, &p);
  EXPECT_EQ(p, LWMQTT_NO_PACKET);
  EXPECT_EQ(err, LWMQTT_MISSING_OR_WRONG_PACKET);
}

TEST(DetectRemainingLength, Valid1) {
  uint8_t h = 60;
  uint32_t rem_len = 0;
  lwmqtt_err_t err = lwmqtt_detect_remaining_length(&h, 1, &rem_len);
  EXPECT_EQ(rem_len, (uint32_t)60);
  EXPECT_EQ(err, LWMQTT_SUCCESS);
}

TEST(DetectRemainingLength, Valid2) {
  uint8_t h[2] = {255, 60};
  uint32_t rem_len = 0;
  lwmqtt_err_t err = lwmqtt_detect_remaining_length(h, 2, &rem_len);
  EXPECT_EQ(rem_len, (uint32_t)7807);
  EXPECT_EQ(err, LWMQTT_SUCCESS);
}

TEST(DetectRemainingLength, ToShort) {
  uint8_t h = 255;
  uint32_t rem_len = 0;
  lwmqtt_err_t err = lwmqtt_detect_remaining_length(&h, 1, &rem_len);
  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(DetectRemainingLength, Overflow) {
  uint8_t h[5] = {255, 255, 255, 255, 255};
  uint32_t rem_len = 0;
  lwmqtt_err_t err = lwmqtt_detect_remaining_length(h, 5, &rem_len);
  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_OVERFLOW);
}

TEST(ConnectTest, Encode1) {
  uint8_t pkt[62] = {
      LWMQTT_CONNECT_PACKET << 4,
      60,
      0,  // Protocol String MSB
      4,  // Protocol String LSB
      'M',
      'Q',
      'T',
      'T',
      4,    // Protocol level 4
      204,  // Connect Flags
      0,    // Keep Alive MSB
      10,   // Keep Alive LSB
      0,    // Client ID MSB
      7,    // Client ID LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,  // Will Topic MSB
      4,  // Will Topic LSB
      'w',
      'i',
      'l',
      'l',
      0,   // Will Message MSB
      12,  // Will Message LSB
      's',
      'e',
      'n',
      'd',
      ' ',
      'm',
      'e',
      ' ',
      'h',
      'o',
      'm',
      'e',
      0,  // Username ID MSB
      7,  // Username ID LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,   // Password ID MSB
      10,  // Password ID LSB
      'v',
      'e',
      'r',
      'y',
      's',
      'e',
      'c',
      'r',
      'e',
      't',
  };

  uint8_t buf[62];

  lwmqtt_will_t will = lwmqtt_default_will;
  will.topic = lwmqtt_string("will");
  will.payload = lwmqtt_string("send me home");
  will.qos = LWMQTT_QOS1;

  lwmqtt_options_t opts = lwmqtt_default_options;
  opts.clean_session = false;
  opts.keep_alive = 10;
  opts.client_id = lwmqtt_string("surgemq");
  opts.username = lwmqtt_string("surgemq");
  opts.password = lwmqtt_string("verysecret");

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_connect(buf, 62, &len, opts, &will);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(ConnectTest, Encode2) {
  uint8_t pkt[14] = {
      LWMQTT_CONNECT_PACKET << 4,
      12,
      0,  // Protocol String MSB
      4,  // Protocol String LSB
      'M',
      'Q',
      'T',
      'T',
      4,   // Protocol level 4
      2,   // Connect Flags
      0,   // Keep Alive MSB
      60,  // Keep Alive LSB
      0,   // Client ID MSB
      0,   // Client ID LSB
  };

  uint8_t buf[14];

  lwmqtt_options_t opts = lwmqtt_default_options;

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_connect(buf, 14, &len, opts, NULL);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(ConnectTest, EncodeError1) {
  uint8_t buf[4];  // <- too small buffer

  lwmqtt_options_t opts = lwmqtt_default_options;

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_connect(buf, 4, &len, opts, NULL);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(ConnackTest, Decode1) {
  uint8_t pkt[4] = {
      LWMQTT_CONNACK_PACKET << 4, 2,
      0,  // session not present
      0,  // connection accepted
  };

  bool session_present;
  lwmqtt_return_code_t return_code;
  lwmqtt_err_t err = lwmqtt_decode_connack(pkt, 4, &session_present, &return_code);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_EQ(session_present, false);
  EXPECT_EQ(return_code, LWMQTT_CONNECTION_ACCEPTED);
}

TEST(ConnackTest, DecodeError1) {
  uint8_t pkt[4] = {
      LWMQTT_CONNACK_PACKET << 4,
      3,  // <-- wrong size
      0,  // session not present
      0,  // connection accepted
  };

  bool session_present;
  lwmqtt_return_code_t return_code;
  lwmqtt_err_t err = lwmqtt_decode_connack(pkt, 4, &session_present, &return_code);

  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_MISMATCH);
}

TEST(ConnackTest, DecodeError2) {
  uint8_t pkt[3] = {
      LWMQTT_CONNACK_PACKET << 4, 3,
      0,  // session not present
          // <- missing packet size
  };

  bool session_present;
  lwmqtt_return_code_t return_code;
  lwmqtt_err_t err = lwmqtt_decode_connack(pkt, 3, &session_present, &return_code);

  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_MISMATCH);
}

TEST(ZeroTest, Encode1) {
  uint8_t pkt[2] = {LWMQTT_PINGREQ_PACKET << 4, 0};

  uint8_t buf[2];

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_zero(buf, 2, &len, LWMQTT_PINGREQ_PACKET);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(AckTest, Decode1) {
  uint8_t pkt[4] = {
      LWMQTT_PUBACK_PACKET << 4, 2,
      0,  // packet ID MSB
      7,  // packet ID LSB
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_err_t err = lwmqtt_decode_ack(pkt, 4, LWMQTT_PUBACK_PACKET, &dup, &packet_id);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_EQ(dup, false);
  EXPECT_EQ(packet_id, 7);
}

TEST(AckTest, DecodeError1) {
  uint8_t pkt[4] = {
      LWMQTT_PUBACK_PACKET << 4,
      1,  // <-- wrong remaining length
      0,  // packet ID MSB
      7,  // packet ID LSB
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_err_t err = lwmqtt_decode_ack(pkt, 4, LWMQTT_PUBACK_PACKET, &dup, &packet_id);

  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_MISMATCH);
}

TEST(AckTest, DecodeError2) {
  uint8_t pkt[3] = {
      LWMQTT_PUBACK_PACKET << 4,
      1,  // <-- wrong remaining length
      0,  // packet ID MSB
          //  <- insufficient bytes
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_err_t err = lwmqtt_decode_ack(pkt, 4, LWMQTT_PUBACK_PACKET, &dup, &packet_id);

  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_MISMATCH);
}

TEST(AckTest, Encode1) {
  uint8_t pkt[4] = {
      LWMQTT_PUBACK_PACKET << 4, 2,
      0,  // packet ID MSB
      7,  // packet ID LSB
  };

  uint8_t buf[4];

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_ack(buf, 4, &len, LWMQTT_PUBACK_PACKET, 0, 7);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(AckTest, EncodeError1) {
  uint8_t buf[2];  // <- too small buffer

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_ack(buf, 2, &len, LWMQTT_PUBACK_PACKET, 0, 7);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(PublishTest, Decode1) {
  uint8_t pkt[25] = {
      LWMQTT_PUBLISH_PACKET << 4 | 11,
      23,
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,  // packet ID MSB
      7,  // packet ID LSB
      's',
      'e',
      'n',
      'd',
      ' ',
      'm',
      'e',
      ' ',
      'h',
      'o',
      'm',
      'e',
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_string_t topic;
  lwmqtt_message_t msg;
  lwmqtt_err_t err = lwmqtt_decode_publish(pkt, 25, &dup, &packet_id, &topic, &msg);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_EQ(dup, true);
  EXPECT_EQ(msg.qos, 1);
  EXPECT_EQ(msg.retained, true);
  EXPECT_EQ(packet_id, 7);
  EXPECT_ARRAY_EQ("surgemq", topic.data, 7);
  EXPECT_EQ(msg.payload_len, (size_t)12);
  EXPECT_ARRAY_EQ("send me home", msg.payload, 12);
}

TEST(PublishTest, Decode2) {
  uint8_t pkt[23] = {
      LWMQTT_PUBLISH_PACKET << 4,
      21,
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      's',
      'e',
      'n',
      'd',
      ' ',
      'm',
      'e',
      ' ',
      'h',
      'o',
      'm',
      'e',
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_string_t topic = lwmqtt_default_string;
  lwmqtt_message_t msg;
  lwmqtt_err_t err = lwmqtt_decode_publish(pkt, 23, &dup, &packet_id, &topic, &msg);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_EQ(dup, false);
  EXPECT_EQ(msg.qos, 0);
  EXPECT_EQ(msg.retained, false);
  EXPECT_EQ(packet_id, 0);
  EXPECT_ARRAY_EQ("surgemq", topic.data, 7);
  EXPECT_EQ(msg.payload_len, (size_t)12);
  EXPECT_ARRAY_EQ("send me home", msg.payload, 12);
}

TEST(PublishTest, DecodeError1) {
  uint8_t pkt[2] = {
      LWMQTT_PUBLISH_PACKET << 4,
      2,  // <-- too much
  };

  bool dup;
  uint16_t packet_id;
  lwmqtt_string_t topic = lwmqtt_default_string;
  lwmqtt_message_t msg;
  lwmqtt_err_t err = lwmqtt_decode_publish(pkt, 2, &dup, &packet_id, &topic, &msg);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(PublishTest, Encode1) {
  uint8_t pkt[25] = {
      LWMQTT_PUBLISH_PACKET << 4 | 11,
      23,
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,  // packet ID MSB
      7,  // packet ID LSB
      's',
      'e',
      'n',
      'd',
      ' ',
      'm',
      'e',
      ' ',
      'h',
      'o',
      'm',
      'e',
  };

  uint8_t buf[25];

  lwmqtt_string_t topic = lwmqtt_string("surgemq");
  lwmqtt_message_t msg = lwmqtt_default_message;
  msg.qos = LWMQTT_QOS1;
  msg.payload = (uint8_t*)"send me home";
  msg.payload_len = 12;
  msg.retained = true;

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_publish(buf, 25, &len, true, 7, topic, msg);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(PublishTest, Encode2) {
  uint8_t pkt[23] = {
      LWMQTT_PUBLISH_PACKET << 4,
      21,
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      's',
      'e',
      'n',
      'd',
      ' ',
      'm',
      'e',
      ' ',
      'h',
      'o',
      'm',
      'e',
  };

  uint8_t buf[23];

  lwmqtt_string_t topic = lwmqtt_string("surgemq");
  lwmqtt_message_t msg = lwmqtt_default_message;
  msg.payload = (uint8_t*)"send me home";
  msg.payload_len = 12;

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_publish(buf, 23, &len, false, 0, topic, msg);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(PublishTest, EncodeError1) {
  uint8_t buf[2];  // <- too small buffer

  lwmqtt_string_t topic = lwmqtt_string("surgemq");
  lwmqtt_message_t msg = lwmqtt_default_message;
  msg.payload = (uint8_t*)"send me home";
  msg.payload_len = 12;

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_publish(buf, 2, &len, false, 0, topic, msg);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(SubackTest, Decode1) {
  uint8_t pkt[8] = {
      LWMQTT_SUBACK_PACKET << 4,
      4,
      0,  // packet ID MSB
      7,  // packet ID LSB
      0,  // return code 1
      1,  // return code 2
  };

  uint16_t packet_id;
  int count;
  lwmqtt_qos_t granted_qos_levels[2];
  lwmqtt_err_t err = lwmqtt_decode_suback(pkt, 8, &packet_id, 2, &count, granted_qos_levels);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_EQ(packet_id, 7);
  EXPECT_EQ(count, 2);
  EXPECT_EQ(granted_qos_levels[0], 0);
  EXPECT_EQ(granted_qos_levels[1], 1);
}

TEST(SubackTest, DecodeError1) {
  uint8_t pkt[5] = {
      LWMQTT_SUBACK_PACKET << 4,
      1,  // <- wrong remaining length
      0,  // packet ID MSB
      7,  // packet ID LSB
      0,  // return code 1
  };

  uint16_t packet_id;
  int count;
  lwmqtt_qos_t granted_qos_levels[2];
  lwmqtt_err_t err = lwmqtt_decode_suback(pkt, 5, &packet_id, 2, &count, granted_qos_levels);

  EXPECT_EQ(err, LWMQTT_REMAINING_LENGTH_MISMATCH);
}

TEST(SubscribeTest, Encode1) {
  uint8_t pkt[38] = {
      LWMQTT_SUBSCRIBE_PACKET << 4 | 2,
      36,
      0,  // packet ID MSB
      7,  // packet ID LSB
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,  // QOS
      0,  // topic name MSB
      8,  // topic name LSB
      '/',
      'a',
      '/',
      'b',
      '/',
      '#',
      '/',
      'c',
      1,   // QOS
      0,   // topic name MSB
      10,  // topic name LSB
      '/',
      'a',
      '/',
      'b',
      '/',
      '#',
      '/',
      'c',
      'd',
      'd',
      2,  // QOS
  };

  uint8_t buf[38];

  lwmqtt_string_t topic_filters[3];
  topic_filters[0] = lwmqtt_string("surgemq");
  topic_filters[1] = lwmqtt_string("/a/b/#/c");
  topic_filters[2] = lwmqtt_string("/a/b/#/cdd");

  lwmqtt_qos_t qos_levels[3] = {LWMQTT_QOS0, LWMQTT_QOS1, LWMQTT_QOS2};

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_subscribe(buf, 38, &len, 7, 3, topic_filters, qos_levels);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(SubscribeTest, EncodeError1) {
  uint8_t buf[2];  // <- too small buffer

  lwmqtt_string_t topic_filters[1];
  topic_filters[0] = lwmqtt_string("surgemq");

  lwmqtt_qos_t qos_levels[1] = {LWMQTT_QOS0};

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_subscribe(buf, 2, &len, 7, 1, topic_filters, qos_levels);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}

TEST(UnsubscribeTest, Encode1) {
  uint8_t pkt[35] = {
      LWMQTT_UNSUBSCRIBE_PACKET << 4 | 2,
      33,
      0,  // packet ID MSB
      7,  // packet ID LSB
      0,  // topic name MSB
      7,  // topic name LSB
      's',
      'u',
      'r',
      'g',
      'e',
      'm',
      'q',
      0,  // topic name MSB
      8,  // topic name LSB
      '/',
      'a',
      '/',
      'b',
      '/',
      '#',
      '/',
      'c',
      0,   // topic name MSB
      10,  // topic name LSB
      '/',
      'a',
      '/',
      'b',
      '/',
      '#',
      '/',
      'c',
      'd',
      'd',
  };

  uint8_t buf[38];

  lwmqtt_string_t topic_filters[3];
  topic_filters[0] = lwmqtt_string("surgemq");
  topic_filters[1] = lwmqtt_string("/a/b/#/c");
  topic_filters[2] = lwmqtt_string("/a/b/#/cdd");

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_unsubscribe(buf, 38, &len, 7, 3, topic_filters);

  EXPECT_EQ(err, LWMQTT_SUCCESS);
  EXPECT_ARRAY_EQ(pkt, buf, len);
}

TEST(UnsubscribeTest, EncodeError1) {
  uint8_t buf[2];  // <- too small buffer

  lwmqtt_string_t topic_filters[1];
  topic_filters[0] = lwmqtt_string("surgemq");

  size_t len;
  lwmqtt_err_t err = lwmqtt_encode_unsubscribe(buf, 2, &len, 7, 1, topic_filters);

  EXPECT_EQ(err, LWMQTT_BUFFER_TOO_SHORT);
}
