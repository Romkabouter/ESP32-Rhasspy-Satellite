#include <lwip/api.h>

// Some docs about netconn: http://www.ecoscentric.com/ecospro/doc/html/ref/lwip-api-sequential-reference.html.

#include "esp_lwmqtt.h"

void esp_lwmqtt_timer_set(void *ref, uint32_t timeout) {
  // cast timer reference
  esp_lwmqtt_timer_t *t = (esp_lwmqtt_timer_t *)ref;

  // set deadline
  t->deadline = (xTaskGetTickCount() * portTICK_PERIOD_MS) + timeout;
}

int32_t esp_lwmqtt_timer_get(void *ref) {
  // cast timer reference
  esp_lwmqtt_timer_t *t = (esp_lwmqtt_timer_t *)ref;

  return (int32_t)t->deadline - (int32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

lwmqtt_err_t esp_lwmqtt_network_connect(esp_lwmqtt_network_t *network, char *host, int port) {
  // disconnect if not already the case
  esp_lwmqtt_network_disconnect(network);

  // resolve address
  ip_addr_t addr;
  err_t err = netconn_gethostbyname_addrtype(host, &addr, NETCONN_DNS_IPV4);
  if (err != ERR_OK) {
    return LWMQTT_NETWORK_FAILED_CONNECT;
  }

  // create new connection
  network->conn = netconn_new(NETCONN_TCP);

  // create new socket
  err = netconn_connect(network->conn, &addr, (u16_t)port);
  if (err != ERR_OK) {
    return LWMQTT_NETWORK_FAILED_CONNECT;
  }

  return LWMQTT_SUCCESS;
}

void esp_lwmqtt_network_disconnect(esp_lwmqtt_network_t *network) {
  // immediately return if conn is not set
  if (network->conn == NULL) {
    return;
  }

  // delete connection
  netconn_delete(network->conn);

  // reset network
  network->conn = NULL;
  network->rest_buf = NULL;
  network->rest_len = 0;
}

lwmqtt_err_t esp_lwmqtt_network_peek(esp_lwmqtt_network_t *network, size_t *available) {
  *available = (size_t)network->conn->recv_avail;
  return LWMQTT_SUCCESS;
}

lwmqtt_err_t esp_lwmqtt_network_read(void *ref, uint8_t *buffer, size_t len, size_t *read, uint32_t timeout) {
  // cast network reference
  esp_lwmqtt_network_t *network = (esp_lwmqtt_network_t *)ref;

  // prepare counter
  size_t copied_len = 0;

  // check if some data is left
  if (network->rest_len > 0) {
    // copy from rest buffer
    netbuf_copy_partial(network->rest_buf, buffer, (u16_t)len,
                        (u16_t)(netbuf_len(network->rest_buf) - network->rest_len));

    // check if there is still data left
    if (network->rest_len > len) {
      network->rest_len -= len;
      *read += len;
      return LWMQTT_SUCCESS;
    }

    // delete rest buffer
    copied_len = network->rest_len;
    netbuf_delete(network->rest_buf);
    network->rest_len = 0;

    // immediately return if we have enough
    if (copied_len == len) {
      *read += len;
      return LWMQTT_SUCCESS;
    }
  }

  // copied_len has the already written amount of data

  // set timeout
  netconn_set_recvtimeout(network->conn, timeout);

  // receive data
  struct netbuf *buf;
  err_t err = netconn_recv(network->conn, &buf);
  if (err == ERR_TIMEOUT) {
    // return zero if timeout has been reached
    *read += copied_len;
    return LWMQTT_SUCCESS;
  } else if (err != ERR_OK) {
    return LWMQTT_NETWORK_FAILED_READ;
  }

  // get length
  size_t bytes = netbuf_len(buf);

  // copy data
  netbuf_copy(buf, buffer + copied_len, len - copied_len);

  // delete buffer and return bytes less or equal to the missing amount
  if (copied_len + bytes <= len) {
    netbuf_delete(buf);
    *read += copied_len + bytes;
    return LWMQTT_SUCCESS;
  }

  // otherwise save the rest and current offset
  network->rest_buf = buf;
  network->rest_len = bytes - (len - copied_len);

  // adjust counter
  *read += len;

  return LWMQTT_SUCCESS;
}

lwmqtt_err_t esp_lwmqtt_network_write(void *ref, uint8_t *buffer, size_t len, size_t *sent, uint32_t timeout) {
  // cast network reference
  esp_lwmqtt_network_t *network = (esp_lwmqtt_network_t *)ref;

  // set timeout
  netconn_set_sendtimeout(network->conn, timeout);

  // send data
  err_t err = netconn_write(network->conn, buffer, len, NETCONN_COPY);
  if (err != ERR_OK) {
    return LWMQTT_NETWORK_FAILED_WRITE;
  }

  // adjust counter
  *sent += len;

  return LWMQTT_SUCCESS;
}
