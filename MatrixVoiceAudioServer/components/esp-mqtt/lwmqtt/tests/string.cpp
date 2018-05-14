#include <gtest/gtest.h>

extern "C" {
#include <lwmqtt.h>
}

TEST(StringConstructor, Valid) {
  lwmqtt_string_t null_str = lwmqtt_string(nullptr);
  EXPECT_EQ(null_str.len, 0);
  EXPECT_TRUE(null_str.data == nullptr);

  lwmqtt_string_t empty_str = lwmqtt_string("");
  EXPECT_EQ(empty_str.len, 0);
  EXPECT_TRUE(empty_str.data == nullptr);

  lwmqtt_string_t hello_str = lwmqtt_string("hello");
  EXPECT_EQ(hello_str.len, 5);
  EXPECT_TRUE(hello_str.data != nullptr);
}

TEST(StringCompare, Valid) {
  lwmqtt_string_t null_str = lwmqtt_string(nullptr);
  EXPECT_TRUE(lwmqtt_strcmp(null_str, nullptr) == 0);
  EXPECT_TRUE(lwmqtt_strcmp(null_str, "") == 0);
  EXPECT_TRUE(lwmqtt_strcmp(null_str, "hello") != 0);

  lwmqtt_string_t empty_str = lwmqtt_string("");
  EXPECT_TRUE(lwmqtt_strcmp(empty_str, nullptr) == 0);
  EXPECT_TRUE(lwmqtt_strcmp(empty_str, "") == 0);
  EXPECT_TRUE(lwmqtt_strcmp(empty_str, "hello") != 0);

  lwmqtt_string_t hello_str = lwmqtt_string("hello");
  EXPECT_TRUE(lwmqtt_strcmp(hello_str, nullptr) != 0);
  EXPECT_TRUE(lwmqtt_strcmp(hello_str, "") != 0);
  EXPECT_TRUE(lwmqtt_strcmp(hello_str, "hello") == 0);
}
