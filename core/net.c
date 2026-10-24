#include "net.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void fern_net_init(FernNet *n) {
    memset(n, 0, sizeof(*n));
    n->fd = -1;
}

static void fail(FernNet *n, const char *what, int err) {
    snprintf(n->error, sizeof(n->error), "%s: %s", what, strerror(err));
    if (n->fd >= 0) close(n->fd);
    n->fd = -1;
    n->state = FERN_NET_FAILED;
}

int fern_net_connect(FernNet *n, const char *host, int port) {
    struct addrinfo hints, *res = NULL;
    char service[16];
    int one = 1, rc;
    fern_net_close(n);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", port);
    rc = getaddrinfo(host, service, &hints, &res);
    if (rc != 0 || !res) {
        snprintf(n->error, sizeof(n->error), "cannot find %s: %s", host, gai_strerror(rc));
        n->state = FERN_NET_FAILED;
        return 0;
    }
    n->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (n->fd < 0) {
        freeaddrinfo(res);
        fail(n, "socket", errno);
        return 0;
    }
    setsockopt(n->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    setsockopt(n->fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    fcntl(n->fd, F_SETFL, fcntl(n->fd, F_GETFL) | O_NONBLOCK);
    rc = connect(n->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (rc != 0 && errno != EINPROGRESS) {
        fail(n, "connect", errno);
        return 0;
    }
    n->state = rc == 0 ? FERN_NET_OPEN : FERN_NET_CONNECTING;
    return 1;
}

static void grow(uint8_t **buf, size_t *cap, size_t need) {
    if (need <= *cap) return;
    size_t c = *cap ? *cap : 4096;
    while (c < need) c *= 2;
    *buf = realloc(*buf, c);
    *cap = c;
}

void fern_net_send(FernNet *n, const void *data, size_t len) {
    grow(&n->out, &n->out_cap, n->out_len + len);
    memcpy(n->out + n->out_len, data, len);
    n->out_len += len;
}

FernNetState fern_net_poll(FernNet *n) {
    if (n->state == FERN_NET_CONNECTING) {
        fd_set w;
        struct timeval tv = {0, 0};
        int err = 0;
        socklen_t len = sizeof(err);
        FD_ZERO(&w);
        FD_SET(n->fd, &w);
        if (select(n->fd + 1, NULL, &w, NULL, &tv) <= 0) return n->state;
        getsockopt(n->fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) {
            fail(n, "connect", err);
            return n->state;
        }
        n->state = FERN_NET_OPEN;
    }
    if (n->state != FERN_NET_OPEN) return n->state;
    while (n->out_len > 0) {
        ssize_t k = send(n->fd, n->out, n->out_len, 0);
        if (k < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            fail(n, "send", errno);
            return n->state;
        }
        memmove(n->out, n->out + k, n->out_len - (size_t)k);
        n->out_len -= (size_t)k;
        n->bytes_out += (uint64_t)k;
    }
    for (;;) {
        ssize_t k;
        grow(&n->in, &n->in_cap, n->in_len + 16384);
        k = recv(n->fd, n->in + n->in_len, n->in_cap - n->in_len, 0);
        if (k > 0) {
            n->in_len += (size_t)k;
            n->bytes_in += (uint64_t)k;
            continue;
        }
        if (k == 0) {
            snprintf(n->error, sizeof(n->error), "the server closed the connection");
            close(n->fd);
            n->fd = -1;
            n->state = FERN_NET_CLOSED;
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
        fail(n, "recv", errno);
        break;
    }
    return n->state;
}

void fern_net_consume(FernNet *n, size_t len) {
    if (len > n->in_len) len = n->in_len;
    memmove(n->in, n->in + len, n->in_len - len);
    n->in_len -= len;
}

void fern_net_close(FernNet *n) {
    if (n->fd >= 0) close(n->fd);
    n->fd = -1;
    n->state = FERN_NET_IDLE;
    n->in_len = 0;
    n->out_len = 0;
}
