#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "client.h"
#include "test.h"

// A fake server on 127.0.0.1. The test plays the server side by hand.
static int listen_on(int *port) {
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

// Run the client until the server socket has n bytes, or 2 seconds pass.
static int pump(FernClient *c, int fd, uint8_t *buf, int n) {
    FernInput in;
    int got = 0, i;
    memset(&in, 0, sizeof(in));
    for (i = 0; i < 2000 && got < n; i++) {
        ssize_t k;
        fern_client_update(c, &in, i * 0.001);
        k = recv(fd, buf + got, (size_t)(n - got), MSG_DONTWAIT);
        if (k > 0) got += (int)k;
        usleep(1000);
    }
    return got;
}

TEST(client_waits_for_the_handshake_then_logs_in_and_answers_keepalive) {
    static FernClient c;
    FernWriter w;
    uint8_t buf[256];
    int port, server, peer;
    server = listen_on(&port);
    fern_client_init(&c);
    fern_client_connect(&c, fern_version_latest(), "127.0.0.1", port, "Ab");
    peer = accept(server, NULL, NULL);
    // The handshake: ID 2 and "Ab" as UTF-16. Nothing more comes yet.
    CHECK_EQ(pump(&c, peer, buf, 7), 7);
    CHECK_EQ(buf[0], 0x02);
    CHECK_EQ(c.state, CLIENT_HANDSHAKE);
    CHECK_EQ(pump(&c, peer, buf, 1), 0);
    fern_writer_init(&w);
    fern_write_u8(&w, 0x02);
    fern_write_str16(&w, "-");
    send(peer, w.data, w.len, 0);
    // The login: ID, protocol 14, name, seed, dimension.
    CHECK_EQ(pump(&c, peer, buf, 20), 20);
    CHECK_EQ(buf[0], 0x01);
    CHECK_EQ(buf[4], 14);
    fern_writer_reset(&w);
    fern_write_u8(&w, 0x01);
    fern_write_i32(&w, 77);
    fern_write_str16(&w, "");
    fern_write_i64(&w, 5);
    fern_write_u8(&w, 0);
    fern_write_u8(&w, 0x00); // A keep-alive in the same read.
    send(peer, w.data, w.len, 0);
    CHECK_EQ(pump(&c, peer, buf, 1), 1);
    CHECK_EQ(buf[0], 0x00);
    CHECK_EQ(c.state, CLIENT_PLAY);
    CHECK_EQ(c.self_id, 77);
    // A kick ends the game with its reason.
    fern_writer_reset(&w);
    fern_write_u8(&w, 0xFF);
    fern_write_str16(&w, "Bye");
    send(peer, w.data, w.len, 0);
    pump(&c, peer, buf, 0);
    pump(&c, peer, buf, 1);
    CHECK_EQ(c.state, CLIENT_GONE);
    CHECK_STR(c.reason, "Bye");
    fern_writer_free(&w);
    fern_client_free(&c);
    close(peer);
    close(server);
}
