#include "net.h"
#include "test.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// Open a listening socket on a free port of 127.0.0.1.
static int listen_local(int *port) {
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(fd, (struct sockaddr *)&a, sizeof(a));
    listen(fd, 1);
    getsockname(fd, (struct sockaddr *)&a, &len);
    *port = ntohs(a.sin_port);
    return fd;
}

TEST(net_sends_and_receives) {
    FernNet n;
    int port, server, client, i;
    char buf[8];
    server = listen_local(&port);
    fern_net_init(&n);
    CHECK(fern_net_connect(&n, "127.0.0.1", port));
    client = accept(server, NULL, NULL);
    fern_net_send(&n, "ping", 4);
    for (i = 0; i < 100 && n.out_len; i++) fern_net_poll(&n);
    CHECK_EQ(n.state, FERN_NET_OPEN);
    CHECK_EQ(recv(client, buf, 4, MSG_WAITALL), 4);
    CHECK(memcmp(buf, "ping", 4) == 0);
    send(client, "pong", 4, 0);
    for (i = 0; i < 1000 && n.in_len < 4; i++) {
        fern_net_poll(&n);
        usleep(1000);
    }
    CHECK_EQ(n.in_len, 4);
    fern_net_consume(&n, 2);
    CHECK(memcmp(n.in, "ng", 2) == 0);
    close(client);
    for (i = 0; i < 1000 && n.state == FERN_NET_OPEN; i++) {
        fern_net_poll(&n);
        usleep(1000);
    }
    CHECK_EQ(n.state, FERN_NET_CLOSED);
    fern_net_close(&n);
    close(server);
}

TEST(net_reports_a_refused_connection) {
    FernNet n;
    int port, server, i;
    server = listen_local(&port);
    close(server);
    fern_net_init(&n);
    fern_net_connect(&n, "127.0.0.1", port);
    for (i = 0; i < 1000 && n.state == FERN_NET_CONNECTING; i++) {
        fern_net_poll(&n);
        usleep(1000);
    }
    CHECK_EQ(n.state, FERN_NET_FAILED);
    CHECK(strstr(n.error, "connect") != NULL);
}
