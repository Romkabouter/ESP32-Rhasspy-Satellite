#ifndef LWMQTT_UNIX_H
#define LWMQTT_UNIX_H

#include <sys/time.h>

#include <lwmqtt.h>

/**
 * The UNIX timer object.
 */
typedef struct { struct timeval end; } lwmqtt_unix_timer_t;

/**
 * Callback to set the UNIX timer object.
 *
 * @see lwmqtt_timer_set_t.
 */
void lwmqtt_unix_timer_set(void *ref, uint32_t timeout);

/**
 * Callback to read the UNIX timer object.
 *
 * @see lwmqtt_timer_get_t.
 */
int32_t lwmqtt_unix_timer_get(void *ref);

/**
 * The UNIX network object.
 */
typedef struct { int socket; } lwmqtt_unix_network_t;

/**
 * Function to establish a UNIX network connection.
 *
 * @param network - The network object.
 * @param host - The host.
 * @param port - The port.
 * @return An error value.
 */
lwmqtt_err_t lwmqtt_unix_network_connect(lwmqtt_unix_network_t *network, char *host, int port);

/**
 * Function to disconnect a UNIX network connection.
 *
 * @param network - The network object.
 */
void lwmqtt_unix_network_disconnect(lwmqtt_unix_network_t *network);

/**
 * Function to peek available bytes on a UNIX network connection.
 *
 * @param network - The network object.
 * @param available - Variables that must be set with the available bytes.
 * @return An error value.
 */
lwmqtt_err_t lwmqtt_unix_network_peek(lwmqtt_unix_network_t *network, size_t *available);

/**
 * Callback to read from a UNIX network connection.
 *
 * @see lwmqtt_network_read_t.
 */
lwmqtt_err_t lwmqtt_unix_network_read(void *ref, uint8_t *buf, size_t len, size_t *read, uint32_t timeout);

/**
 * Callback to write to a UNIX network connection.
 *
 * @see lwmqtt_network_write_t.
 */
lwmqtt_err_t lwmqtt_unix_network_write(void *ref, uint8_t *buf, size_t len, size_t *sent, uint32_t timeout);

#endif  // LWMQTT_UNIX_H
