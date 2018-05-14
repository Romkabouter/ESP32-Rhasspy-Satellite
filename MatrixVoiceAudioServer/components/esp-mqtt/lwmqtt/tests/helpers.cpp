#include <gtest/gtest.h>

extern "C" {
#include "../src/helpers.h"
}

TEST(VarNum1, Valid) {
  uint8_t buf[2];
  memset(buf, 0, 2);

  for (uint32_t i = 1; i < 128; i++) {
    int len;
    lwmqtt_err_t err = lwmqtt_varnum_length(i, &len);
    EXPECT_EQ(err, LWMQTT_SUCCESS);
    EXPECT_EQ(len, 1);

    uint8_t *ptr1 = buf;
    err = lwmqtt_write_varnum(&ptr1, buf + 1, i);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    uint8_t *ptr2 = buf;
    uint32_t num;
    err = lwmqtt_read_varnum(&ptr2, buf + 1, &num);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    EXPECT_EQ(num, i);
    EXPECT_EQ(buf[1], 0);
    EXPECT_EQ(ptr1 - buf, 1);
    EXPECT_EQ(ptr2 - buf, 1);
  }
}

TEST(VarNum2, Valid) {
  uint8_t buf[3];
  memset(buf, 0, 3);

  for (uint32_t i = 1; i < 128; i++) {
    int len;
    lwmqtt_err_t err = lwmqtt_varnum_length(128 * i, &len);
    EXPECT_EQ(err, LWMQTT_SUCCESS);
    EXPECT_EQ(len, 2);

    uint8_t *ptr1 = buf;
    err = lwmqtt_write_varnum(&ptr1, buf + 2, 128 * i);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    uint8_t *ptr2 = buf;
    uint32_t num;
    err = lwmqtt_read_varnum(&ptr2, buf + 2, &num);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    EXPECT_EQ(num, 128 * i);
    EXPECT_EQ(buf[2], 0);
    EXPECT_EQ(ptr1 - buf, 2);
    EXPECT_EQ(ptr2 - buf, 2);
  }
}

TEST(VarNum3, Valid) {
  uint8_t buf[4];
  memset(buf, 0, 4);

  for (uint32_t i = 1; i < 128; i++) {
    int len;
    lwmqtt_err_t err = lwmqtt_varnum_length(128 * 128 * i, &len);
    EXPECT_EQ(err, LWMQTT_SUCCESS);
    EXPECT_EQ(len, 3);

    uint8_t *ptr1 = buf;
    err = lwmqtt_write_varnum(&ptr1, buf + 3, 128 * 128 * i);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    uint8_t *ptr2 = buf;
    uint32_t num;
    err = lwmqtt_read_varnum(&ptr2, buf + 3, &num);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    EXPECT_EQ(num, 128 * 128 * i);
    EXPECT_EQ(buf[3], 0);
    EXPECT_EQ(ptr1 - buf, 3);
    EXPECT_EQ(ptr2 - buf, 3);
  }
}

TEST(VarNum4, Valid) {
  uint8_t buf[5];
  memset(buf, 0, 5);

  for (uint32_t i = 1; i < 128; i++) {
    int len;
    lwmqtt_err_t err = lwmqtt_varnum_length(128 * 128 * 128 * i, &len);
    EXPECT_EQ(err, LWMQTT_SUCCESS);
    EXPECT_EQ(len, 4);

    uint8_t *ptr1 = buf;
    err = lwmqtt_write_varnum(&ptr1, buf + 4, 128 * 128 * 128 * i);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    uint8_t *ptr2 = buf;
    uint32_t num;
    err = lwmqtt_read_varnum(&ptr2, buf + 4, &num);
    EXPECT_EQ(err, LWMQTT_SUCCESS);

    EXPECT_EQ(num, 128 * 128 * 128 * i);
    EXPECT_EQ(buf[4], 0);
    EXPECT_EQ(ptr1 - buf, 4);
    EXPECT_EQ(ptr2 - buf, 4);
  }
}

TEST(VarNumMax, Valid) {
  uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0x7F};

  uint8_t *ptr = buf;
  uint32_t num;
  lwmqtt_err_t err = lwmqtt_read_varnum(&ptr, buf + 4, &num);
  EXPECT_EQ(err, LWMQTT_SUCCESS);

  EXPECT_EQ(num, (uint32_t)268435455);
}
