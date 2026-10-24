// net.h: a TCP connection that does not block.
// The game calls fern_net_poll once in each frame. It sends the bytes that
// wait, and it reads the bytes that came in.
#ifndef FERN_NET_H
#define FERN_NET_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FERN_NET_IDLE,
    FERN_NET_CONNECTING,
    FERN_NET_OPEN,
    FERN_NET_CLOSED,
    FERN_NET_FAILED
} FernNetState;

typedef struct {
    int fd;
    FernNetState state;
    char error[128];
    uint8_t *in;
    size_t in_len, in_cap;
    uint8_t *out;
    size_t out_len, out_cap;
    uint64_t bytes_in, bytes_out;
} FernNet;

void fern_net_init(FernNet *n);
// Start a connection to host:port. host is an IP address or a name.
int fern_net_connect(FernNet *n, const char *host, int port);
// Add bytes to the queue that goes out.
void fern_net_send(FernNet *n, const void *data, size_t len);
// Send and receive. Give the state after the poll.
FernNetState fern_net_poll(FernNet *n);
// Remove the first len bytes of the input.
void fern_net_consume(FernNet *n, size_t len);
void fern_net_close(FernNet *n);

#endif
