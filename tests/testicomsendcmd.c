/*
 * Hamlib Icom raw CI-V send_cmd/send_cmd_rx passthrough tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "misc.h"

#include "rigctl_parse.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

extern struct rig_caps ic7100_caps;
extern struct rig_caps ic7300_caps;

static int close_test_socket(int fd)
{
#ifdef _WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
}

static int read_test_socket(int fd, void *buffer, size_t length)
{
#ifdef _WIN32
    return recv(fd, buffer, (int) length, 0);
#else
    return (int) read(fd, buffer, length);
#endif
}

static int write_test_socket(int fd, const void *buffer, size_t length)
{
#ifdef _WIN32
    return send(fd, buffer, (int) length, 0);
#else
    return (int) write(fd, buffer, length);
#endif
}

static int open_test_connection(int sockets[2])
{
#ifdef _WIN32
    SOCKET listener;
    SOCKET client;
    SOCKET peer;
    struct sockaddr_in address;
    int address_length = sizeof(address);

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) { return -1; }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(listener, (struct sockaddr *) &address, sizeof(address)) != 0 ||
            listen(listener, 1) != 0 ||
            getsockname(listener, (struct sockaddr *) &address,
                        &address_length) != 0)
    {
        closesocket(listener);
        return -1;
    }

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client == INVALID_SOCKET ||
            connect(client, (struct sockaddr *) &address,
                    sizeof(address)) != 0)
    {
        if (client != INVALID_SOCKET) { closesocket(client); }

        closesocket(listener);
        return -1;
    }

    peer = accept(listener, NULL, NULL);
    closesocket(listener);

    if (peer == INVALID_SOCKET)
    {
        closesocket(client);
        return -1;
    }

    sockets[0] = (int) client;
    sockets[1] = (int) peer;
    return 0;
#else
    return socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
#endif
}

static int read_frame(int fd, unsigned char *frame, size_t capacity)
{
    size_t length = 0;

    while (length < capacity)
    {
        int count = read_test_socket(fd, frame + length, 1);

        if (count != 1)
        {
            return -1;
        }

        if (frame[length++] == 0xfd)
        {
            return (int) length;
        }
    }

    return -1;
}

static int write_all(int fd, const unsigned char *buffer, size_t length)
{
    size_t written = 0;

    while (written < length)
    {
        int count = write_test_socket(fd, buffer + written, length - written);

        if (count <= 0)
        {
            return -1;
        }

        written += (size_t) count;
    }

    return 0;
}

struct scripted_peer
{
    const char *name;
    const unsigned char *request;
    size_t request_len;
    const unsigned char *const *frames;
    const size_t *frame_lens;
    size_t frame_count;
    int fd;
    int status;
};

static void *run_peer(void *arg)
{
    struct scripted_peer *test = arg;
    unsigned char request[64];
    int request_len;
    size_t i;

    test->status = -1;
    request_len = read_frame(test->fd, request, sizeof(request));

    if (request_len != (int) test->request_len
            || memcmp(request, test->request, test->request_len) != 0)
    {
        fprintf(stderr, "%s: unexpected request frame\n", test->name);
        return NULL;
    }

    for (i = 0; i < test->frame_count; i++)
    {
        if (write_all(test->fd, test->frames[i], test->frame_lens[i]) != 0)
        {
            fprintf(stderr, "%s: failed writing frame %zu\n", test->name,
                    i + 1);
            return NULL;
        }
    }

    test->status = 0;
    return NULL;
}

static RIG *prepare_rig(rig_model_t model, int fd)
{
    RIG *rig = rig_init(model);

    if (rig == NULL)
    {
        return NULL;
    }

    STATE(rig)->comm_state = 1;
    RIGPORT(rig)->fd = fd;
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->retry = 0;
    RIGPORT(rig)->timeout = 200;
    /* normally set by rig_open(), which this harness deliberately skips;
     * send_cmd()'s simulate check tests this backpointer, not type.rig */
    RIGPORT(rig)->rig = rig;
    return rig;
}

static void release_rig(RIG *rig, int fd)
{
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    close_test_socket(fd);
    rig_cleanup(rig);
}

/* Same pattern as tests/testctlparser.c's parse_command_output(). */
static int parse_command_output(RIG *rig, char *argv[], int argc,
                                unsigned char *output, size_t output_size,
                                size_t *output_length)
{
    FILE *input = tmpfile();
    FILE *stream = tmpfile();
    int vfo_mode = 0;
    int ext_resp = 0;
    char resp_sep = '\n';
    int ret;

    *output_length = 0;

    if (input == NULL || stream == NULL)
    {
        if (input != NULL) { fclose(input); }

        if (stream != NULL) { fclose(stream); }

        return -RIG_EINTERNAL;
    }

    optind = 1;
    ret = rigctl_parse(rig, input, stream, argv, argc, NULL, 0, 0,
                       &vfo_mode, -1, &ext_resp, &resp_sep, 0);
    rewind(stream);
    *output_length = fread(output, 1, output_size, stream);
    fclose(input);
    fclose(stream);
    return ret;
}

/*
 * Builds the "\0xAA\0xBB..." hex-escaped form send_cmd's binary decoder and
 * encoder both use -- for encoding the raw frame passed as the command
 * argument, and for building the expected decoded reply to compare against.
 */
static void hex_escape(const unsigned char *bytes, size_t len, char *out,
                       size_t out_size)
{
    size_t pos = 0;
    size_t i;

    out[0] = '\0';

    for (i = 0; i < len && pos + 6 < out_size; i++)
    {
        SNPRINTF(out + pos, out_size - pos, "\\0x%02X", bytes[i]);
        pos += 5;
    }
}

static int run_case(const char *name, rig_model_t model,
                    const unsigned char *request, size_t request_len,
                    const unsigned char *const *frames,
                    const size_t *frame_lens, size_t frame_count,
                    int expected_retval, const unsigned char *expected_reply,
                    size_t expected_reply_len)
{
    int sockets[2];
    pthread_t thread;
    struct scripted_peer test =
    {
        .name = name,
        .request = request,
        .request_len = request_len,
        .frames = frames,
        .frame_lens = frame_lens,
        .frame_count = frame_count,
        .fd = -1,
        .status = -1
    };
    RIG *rig;
    char command[256];
    char *argv[] = { "testicomsendcmd", "W", command, "64" };
    unsigned char actual[256];
    size_t actual_length = 0;
    int ret;

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "%s: test socket setup failed\n", name);
        return 1;
    }

    test.fd = sockets[1];

    if (pthread_create(&thread, NULL, run_peer, &test) != 0)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        return 1;
    }

    rig = prepare_rig(model, sockets[0]);

    if (rig == NULL)
    {
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        pthread_join(thread, NULL);
        return 1;
    }

    hex_escape(request, request_len, command, sizeof(command));
    ret = parse_command_output(rig, argv, 4, actual, sizeof(actual),
                               &actual_length);
    release_rig(rig, sockets[0]);
    pthread_join(thread, NULL);
    close_test_socket(sockets[1]);

    if (test.status != 0)
    {
        fprintf(stderr, "%s: peer did not complete its script\n", name);
        return 1;
    }

    if (ret != expected_retval)
    {
        fprintf(stderr, "%s: expected return %d, got %d\n", name,
                expected_retval, ret);
        return 1;
    }

    if (expected_reply != NULL)
    {
        char expected_text[300];
        size_t expected_text_len;

        hex_escape(expected_reply, expected_reply_len, expected_text,
                   sizeof(expected_text));
        expected_text_len = strlen(expected_text);
        SNPRINTF(expected_text + expected_text_len,
                 sizeof(expected_text) - expected_text_len, " %d\n",
                 (int) expected_reply_len);
        expected_text_len = strlen(expected_text);

        if (actual_length != expected_text_len
                || memcmp(actual, expected_text, expected_text_len) != 0)
        {
            fprintf(stderr, "%s: unexpected decoded reply\n", name);
            return 1;
        }
    }

    return 0;
}

int main(void)
{
#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    static const unsigned char req_a[] = { 0xfe, 0xfe, 0x88, 0xe0, 0x03, 0xfd };
    static const unsigned char resp_a[] =
    {
        0xfe, 0xfe, 0xe0, 0x88, 0x03, 0x01, 0x02, 0x03, 0xfd
    };
    static const unsigned char *const frames_a[] = { req_a, resp_a };
    static const size_t frame_lens_a[] = { sizeof(req_a), sizeof(resp_a) };

    static const unsigned char req_b[] = { 0xfe, 0xfe, 0x94, 0xe0, 0x03, 0xfd };
    static const unsigned char resp_b[] =
    {
        0xfe, 0xfe, 0xe0, 0x94, 0x03, 0x0a, 0x0b, 0xfd
    };
    static const unsigned char *const frames_b[] = { resp_b };
    static const size_t frame_lens_b[] = { sizeof(resp_b) };

    // an echo and a frame for another controller both precede the response; both must be discarded
    static const unsigned char req_c[] =
    {
        0xfe, 0xfe, 0x88, 0xe0, 0x26, 0x00, 0xfd
    };
    static const unsigned char foreign_c[] =
    {
        0xfe, 0xfe, 0x01, 0x94, 0x1c, 0x03, 0x00, 0xfd
    };
    static const unsigned char resp_c[] =
    {
        0xfe, 0xfe, 0xe0, 0x88, 0x26, 0x00, 0x01, 0x00, 0x01, 0xfd
    };
    static const unsigned char *const frames_c[] = { req_c, foreign_c, resp_c };
    static const size_t frame_lens_c[] =
    {
        sizeof(req_c), sizeof(foreign_c), sizeof(resp_c)
    };

    static const unsigned char req_d[] = { 0xfe, 0xfe, 0x88, 0xe1, 0x03, 0xfd };
    static const unsigned char resp_d[] =
    {
        0xfe, 0xfe, 0xe1, 0x88, 0x03, 0x09, 0x08, 0xfd
    };
    static const unsigned char *const frames_d[] = { req_d, resp_d };
    static const size_t frame_lens_d[] = { sizeof(req_d), sizeof(resp_d) };

    // more discardable frames than the bound allows -- must give up with -RIG_EPROTO, not hang
    static const unsigned char req_e[] =
    {
        0xfe, 0xfe, 0x88, 0xe0, 0x26, 0x00, 0xfd
    };
    static const unsigned char *const frames_e[] =
    {
        req_e, req_e, req_e, req_e, req_e, req_e, req_e, req_e, req_e
    };
    static const size_t frame_lens_e[] =
    {
        sizeof(req_e), sizeof(req_e), sizeof(req_e), sizeof(req_e),
        sizeof(req_e), sizeof(req_e), sizeof(req_e), sizeof(req_e),
        sizeof(req_e)
    };

    rig_register(&ic7100_caps);
    rig_register(&ic7300_caps);

    if (run_case("single echo, default controller", RIG_MODEL_IC7100,
                req_a, sizeof(req_a), frames_a, frame_lens_a,
                ARRAY_SIZE(frames_a), RIG_OK, resp_a, sizeof(resp_a)) != 0
            || run_case("no echo", RIG_MODEL_IC7300, req_b, sizeof(req_b),
                       frames_b, frame_lens_b, ARRAY_SIZE(frames_b), RIG_OK,
                       resp_b, sizeof(resp_b)) != 0
            || run_case("multiple discardable frames", RIG_MODEL_IC7100,
                       req_c, sizeof(req_c), frames_c, frame_lens_c,
                       ARRAY_SIZE(frames_c), RIG_OK, resp_c,
                       sizeof(resp_c)) != 0
            || run_case("non-default controller id", RIG_MODEL_IC7100,
                       req_d, sizeof(req_d), frames_d, frame_lens_d,
                       ARRAY_SIZE(frames_d), RIG_OK, resp_d,
                       sizeof(resp_d)) != 0
            || run_case("gives up past the discard bound", RIG_MODEL_IC7100,
                       req_e, sizeof(req_e), frames_e, frame_lens_e,
                       ARRAY_SIZE(frames_e), -RIG_EPROTO, NULL, 0) != 0)
    {
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
