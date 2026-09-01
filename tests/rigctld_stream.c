/*
 *  Hamlib streaming subsystem
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* UDP streaming protocol for rigctld — registry, feeders, lifecycle management. */
/* Wire format pack/unpack functions are in src/stream_proto.c (part of libhamlib). */

/* Must precede the first (transitive) <stdlib.h> so the Windows CRT
 * declares rand_s(), used for the subscribe token. */
#define _CRT_RAND_S

#ifdef HAVE_CONFIG_H
#  include "hamlib/config.h"
#endif

#include "rigctld_stream.h"
#include "rigctld_client.h"
#include "stream_convert.h"
#include "stream.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/* Portable socket headers and socket_close() come from stream_proto.h
 * (pulled in via rigctld_stream.h). */

#ifndef MSG_DONTWAIT
#  define MSG_DONTWAIT 0
#endif

/* Global stream registry (initialized by rigctld main). */
struct rigctld_stream_registry g_stream_registry;

#ifdef _WIN32
/* Winsock rejects every socket/getaddrinfo call with WSANOTINITIALISED
 * until WSAStartup has run. This constructor runs before main() in each
 * program that links this file (rigctld and the unit-test binaries), so
 * it is the single initialization point; rigctld's own WSAStartup for
 * its TCP listener is refcounted alongside it harmlessly. */
static void __attribute__((constructor)) stream_socket_sys_init(void)
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
}
#endif

/* Thread-local client ID storage. */
static pthread_key_t client_id_key;

void rigctld_client_id_init(void)
{
    pthread_key_create(&client_id_key, NULL);
}

void rigctld_client_id_set(int client_id)
{
    pthread_setspecific(client_id_key, (void *)(intptr_t)client_id);
}

int rigctld_client_id_get(void)
{
    return (int)(intptr_t)pthread_getspecific(client_id_key);
}


/* Extract the family and raw IP bytes from a sockaddr_storage, folding
 * IPv4-mapped IPv6 (::ffff:A.B.C.D) to plain IPv4 so dual-stack sockets
 * compare correctly across TCP/UDP boundaries. Returns the address length
 * in bytes, or 0 for unsupported/unset families. All field access goes
 * through memcpy into properly typed locals: an earlier version wrote the
 * normalized address through a sockaddr_in pointer into a
 * sockaddr_storage object and re-read it as sockaddr_storage, a strict-
 * aliasing violation GCC miscompiles at -O2 (the re-read family became
 * AF_UNSPEC, so an IPv4 TCP client never matched its own IPv4-mapped UDP
 * source and every subscribe was rejected). */
static size_t sockaddr_ip_bytes(const struct sockaddr_storage *ss,
                                int *family, unsigned char ip[16])
{
    if (ss->ss_family == AF_INET)
    {
        struct sockaddr_in s4;
        memcpy(&s4, ss, sizeof(s4));
        memcpy(ip, &s4.sin_addr, 4);
        *family = AF_INET;
        return 4;
    }

    if (ss->ss_family == AF_INET6)
    {
        struct sockaddr_in6 s6;
        memcpy(&s6, ss, sizeof(s6));

        if (IN6_IS_ADDR_V4MAPPED(&s6.sin6_addr))
        {
            memcpy(ip, &s6.sin6_addr.s6_addr[12], 4);
            *family = AF_INET;
            return 4;
        }

        memcpy(ip, &s6.sin6_addr, 16);
        *family = AF_INET6;
        return 16;
    }

    *family = AF_UNSPEC;
    return 0;
}


/* Compare IP addresses only (ignoring port) from two sockaddr_storage.
 * Returns 0 if they match, non-zero otherwise.
 * Returns non-zero if either address has family AF_UNSPEC (zeroed). */
static int sockaddr_ip_cmp(const struct sockaddr_storage *a,
                           const struct sockaddr_storage *b)
{
    unsigned char ia[16], ib[16];
    int fa, fb;
    size_t la = sockaddr_ip_bytes(a, &fa, ia);
    size_t lb = sockaddr_ip_bytes(b, &fb, ib);

    if (la == 0 || lb == 0 || fa != fb)
    {
        return -1;
    }

    return memcmp(ia, ib, la);
}


/* Validate a subscribe/control packet against the stream's token and
 * the TCP client IP captured at stream_open.
 * Returns 0 if valid, -1 on token or IP mismatch. */
static int validate_subscribe_packet(struct rigctld_stream *stream,
                                     const struct rig_stream_packet_header *hdr,
                                     const struct sockaddr_storage *from_addr)
{
    /* The stream source ID is meaningful only in the published direction;
     * a client MUST send 0 (reject rather than ingest, like unknown
     * control bits). */
    if (hdr->source_id != 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: non-zero source_id %u from client for stream %d\n",
                  __func__, hdr->source_id, stream->stream_id);
        return -1;
    }

    if (hdr->subscribe_token != stream->subscribe_token)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: invalid token for stream %d: got %u, expected %u\n",
                  __func__, stream->stream_id,
                  hdr->subscribe_token, stream->subscribe_token);
        return -1;
    }

    /* Skip IP check if tcp_client_addr was not captured (test harness) */
    if (stream->tcp_client_addr.ss_family != AF_UNSPEC
            && sockaddr_ip_cmp(&stream->tcp_client_addr, from_addr) != 0)
    {
        char expected[INET6_ADDRSTRLEN] = "?";
        char got[INET6_ADDRSTRLEN] = "?";
        getnameinfo((struct sockaddr *)&stream->tcp_client_addr,
                    sizeof(stream->tcp_client_addr),
                    expected, sizeof(expected), NULL, 0, NI_NUMERICHOST);
        getnameinfo((struct sockaddr *)from_addr,
                    sizeof(*from_addr),
                    got, sizeof(got), NULL, 0, NI_NUMERICHOST);
        rig_debug(RIG_DEBUG_WARN,
                  "%s: IP mismatch for stream %d: expected %s, got %s\n",
                  __func__, stream->stream_id, expected, got);
        return -1;
    }

    return 0;
}


/* --- Stream allocation --- */

struct rigctld_stream *rigctld_stream_alloc(void)
{
    struct rigctld_stream *s = calloc(1, sizeof(*s));

    if (s == NULL)
    {
        return NULL;
    }

    s->udp_sock = -1;
    s->subscribe_timeout_s = RIGCTLD_SUBSCRIBE_TIMEOUT_DEFAULT;

    return s;
}


void rigctld_stream_free(struct rigctld_stream *stream)
{
    free(stream);
}


/* --- Stream registry --- */

int rigctld_stream_registry_init(struct rigctld_stream_registry *reg)
{
    memset(reg->streams, 0, sizeof(reg->streams));
    reg->next_id = 1;
    reg->metadata_interval_ms = RIGCTLD_METADATA_INTERVAL_DEFAULT;
    reg->metadata_refresh_ms = RIGCTLD_METADATA_REFRESH_DEFAULT;
    reg->transport_buffer_ms = RIG_STREAM_TRANSPORT_BUFFER_DURATION_MS;
    reg->transport_buffer_bytes = 0;
    reg->multicast_ttl = 1;
    reg->keepalive_timeout_s = RIGCTLD_SUBSCRIBE_TIMEOUT_DEFAULT;

    if (pthread_mutex_init(&reg->lock, NULL) != 0)
    {
        return -1;
    }

    reg->initialized = 1;

    return 0;
}


void rigctld_stream_registry_destroy(struct rigctld_stream_registry *reg)
{
    int t, i;

    /* Collect all streams, then stop feeders outside lock to avoid deadlock */
    struct rigctld_stream *all[RIG_STREAM_TYPE_COUNT * RIGCTLD_MAX_STREAMS];
    int count = 0;

    pthread_mutex_lock(&reg->lock);

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            if (reg->streams[t][i] != NULL)
            {
                all[count++] = reg->streams[t][i];
                reg->streams[t][i] = NULL;
            }
        }
    }

    pthread_mutex_unlock(&reg->lock);

    for (i = 0; i < count; i++)
    {
        rigctld_stream_feeder_stop(all[i]);
        rigctld_stream_cleanup_resources(all[i]);
    }

    pthread_mutex_destroy(&reg->lock);
    reg->initialized = 0;
}


int rigctld_stream_registry_insert(struct rigctld_stream_registry *reg,
                                   struct rigctld_stream *stream)
{
    int type = (int)stream->type;
    int i;

    if (type < 0 || type >= RIG_STREAM_TYPE_COUNT)
    {
        return -1;
    }

    pthread_mutex_lock(&reg->lock);

    for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
    {
        if (reg->streams[type][i] == NULL)
        {
            reg->streams[type][i] = stream;
            pthread_mutex_unlock(&reg->lock);
            return 0;
        }
    }

    pthread_mutex_unlock(&reg->lock);
    return -1;  /* No free slot */
}


struct rigctld_stream *rigctld_stream_registry_lookup(
    struct rigctld_stream_registry *reg,
    rig_stream_type_t type, int stream_id)
{
    int t = (int)type;
    int i;

    if (t < 0 || t >= RIG_STREAM_TYPE_COUNT)
    {
        return NULL;
    }

    pthread_mutex_lock(&reg->lock);

    for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
    {
        if (reg->streams[t][i] != NULL
                && reg->streams[t][i]->stream_id == stream_id)
        {
            struct rigctld_stream *found = reg->streams[t][i];
            pthread_mutex_unlock(&reg->lock);
            return found;
        }
    }

    pthread_mutex_unlock(&reg->lock);
    return NULL;
}


void rigctld_stream_registry_lock(struct rigctld_stream_registry *reg)
{
    pthread_mutex_lock(&reg->lock);
}


void rigctld_stream_registry_unlock(struct rigctld_stream_registry *reg)
{
    pthread_mutex_unlock(&reg->lock);
}


struct rigctld_stream *rigctld_stream_registry_find_by_id_unlocked(
    struct rigctld_stream_registry *reg, int stream_id)
{
    int t, i;

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            if (reg->streams[t][i] != NULL
                    && reg->streams[t][i]->stream_id == stream_id)
            {
                return reg->streams[t][i];
            }
        }
    }

    return NULL;
}


struct rigctld_stream *rigctld_stream_registry_find_by_id(
    struct rigctld_stream_registry *reg, int stream_id)
{
    struct rigctld_stream *found;

    pthread_mutex_lock(&reg->lock);
    found = rigctld_stream_registry_find_by_id_unlocked(reg, stream_id);
    pthread_mutex_unlock(&reg->lock);

    return found;
}


struct rigctld_stream *rigctld_stream_registry_find_remove_by_id(
    struct rigctld_stream_registry *reg, int stream_id,
    int client_id, int *err)
{
    int t, i;

    pthread_mutex_lock(&reg->lock);

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            if (reg->streams[t][i] != NULL
                    && reg->streams[t][i]->stream_id == stream_id)
            {
                struct rigctld_stream *found = reg->streams[t][i];

                if (found->client_id != client_id)
                {
                    pthread_mutex_unlock(&reg->lock);

                    if (err) { *err = -RIG_EACCESS; }

                    return NULL;
                }

                reg->streams[t][i] = NULL;
                pthread_mutex_unlock(&reg->lock);

                if (err) { *err = RIG_OK; }

                return found;
            }
        }
    }

    pthread_mutex_unlock(&reg->lock);

    if (err) { *err = -RIG_EINVAL; }

    return NULL;
}


struct rigctld_stream *rigctld_stream_registry_remove(
    struct rigctld_stream_registry *reg,
    rig_stream_type_t type, int stream_id)
{
    int t = (int)type;
    int i;

    if (t < 0 || t >= RIG_STREAM_TYPE_COUNT)
    {
        return NULL;
    }

    pthread_mutex_lock(&reg->lock);

    for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
    {
        if (reg->streams[t][i] != NULL
                && reg->streams[t][i]->stream_id == stream_id)
        {
            struct rigctld_stream *removed = reg->streams[t][i];
            reg->streams[t][i] = NULL;
            pthread_mutex_unlock(&reg->lock);
            return removed;
        }
    }

    pthread_mutex_unlock(&reg->lock);
    return NULL;
}


/* Close backend stream, UDP socket, and free memory.
 * Does NOT stop the feeder thread — caller must handle that. */
void rigctld_stream_cleanup_resources(struct rigctld_stream *stream)
{
    if (stream->backend_stream && stream->rig)
    {
        rig_stream_close(stream->rig, stream->backend_stream);
    }

    if (stream->udp_sock >= 0)
    {
        socket_close(stream->udp_sock);
        stream->udp_sock = -1;
    }

    rigctld_stream_free(stream);
}


int rigctld_stream_registry_close_by_client(struct rigctld_stream_registry *reg,
        int client_id)
{
    struct rigctld_stream *to_close[RIGCTLD_MAX_STREAMS * RIG_STREAM_TYPE_COUNT];
    int close_count = 0;
    int t, i;

    /* Gather matching streams under lock */
    pthread_mutex_lock(&reg->lock);

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            if (reg->streams[t][i] != NULL
                    && reg->streams[t][i]->client_id == client_id)
            {
                to_close[close_count++] = reg->streams[t][i];
                reg->streams[t][i] = NULL;
            }
        }
    }

    pthread_mutex_unlock(&reg->lock);

    /* Full cleanup outside lock (feeder_stop may block on pthread_join) */
    for (i = 0; i < close_count; i++)
    {
        rigctld_stream_feeder_stop(to_close[i]);
        rigctld_stream_cleanup_resources(to_close[i]);
    }

    return close_count;
}


int rigctld_stream_registry_count(struct rigctld_stream_registry *reg)
{
    int t, i;
    int count = 0;

    pthread_mutex_lock(&reg->lock);

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            if (reg->streams[t][i] != NULL)
            {
                count++;
            }
        }
    }

    pthread_mutex_unlock(&reg->lock);
    return count;
}


int rigctld_stream_udp_socket_create(int *sock_fd, int *port)
{
    int fd;
    struct sockaddr_in6 addr;
    socklen_t addrlen;

    if (!sock_fd || !port)
    {
        return -1;
    }

    fd = socket(AF_INET6, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return -1;
    }

    /* Allow IPv4 connections on IPv6 socket */
    int off = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
               (const char *)&off, sizeof(off));

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = 0;  /* Ephemeral port */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        socket_close(fd);
        return -1;
    }

    /* Retrieve the assigned port */
    addrlen = sizeof(addr);

    if (getsockname(fd, (struct sockaddr *)&addr, &addrlen) < 0)
    {
        socket_close(fd);
        return -1;
    }

    *sock_fd = fd;
    *port = ntohs(addr.sin6_port);
    return 0;
}


int rigctld_stream_registry_next_id(struct rigctld_stream_registry *reg)
{
    int id;
    int attempts = 0;

    pthread_mutex_lock(&reg->lock);

    do
    {
        id = reg->next_id++;

        if (reg->next_id > 65535)
        {
            reg->next_id = 1;
        }

        if (rigctld_stream_registry_find_by_id_unlocked(reg, id) == NULL)
        {
            break;
        }

        attempts++;
    }
    while (attempts < 65535);

    pthread_mutex_unlock(&reg->lock);

    return (attempts < 65535) ? id : -1;
}


uint32_t rigctld_stream_generate_token(void)
{
    uint32_t token = 0;

#ifdef _WIN32
    /* rand_s draws from the Windows system CSPRNG (RtlGenRandom). */
    unsigned int v = 0;

    if (rand_s(&v) == 0)
    {
        token = (uint32_t)v;
    }

#else

    FILE *fp = fopen("/dev/urandom", "rb");

    if (fp != NULL)
    {
        if (fread(&token, sizeof(token), 1, fp) != 1)
        {
            token = 0;
        }

        fclose(fp);
    }

#endif

    if (token == 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: system RNG unavailable; subscribe token is weak\n",
                  __func__);
        /* Reserve 0 as the "unset" sentinel; hand out 1 as a last resort. */
        token = 1;
    }

    return token;
}


int rigctld_stream_config_from_args(const char *type_str,
                                    const char *format_str,
                                    const char *rate_str,
                                    struct rig_stream_config *config)
{
    rig_stream_type_t type;
    rig_stream_format_t format;
    int rate;

    if (!type_str || !format_str || !rate_str || !config)
    {
        return -1;
    }

    if (stream_type_parse(type_str, &type) < 0)
    {
        return -1;
    }

    format = stream_format_parse(format_str);

    if (format == 0)
    {
        return -1;
    }

    {
        char *endptr;
        long val;
        errno = 0;
        val = strtol(rate_str, &endptr, 10);

        if (endptr == rate_str || *endptr != '\0' || errno != 0
                || val <= 0 || val > 20000000)
        {
            return -1;
        }

        rate = (int)val;
    }

    memset(config, 0, sizeof(*config));
    config->struct_size = sizeof(*config);  /* daemon-local, same-build config */
    config->type = type;
    config->format = format;
    config->sample_rate = rate;
    config->channels = 1;

    return 0;
}


/* --- Multicast support --- */


int rigctld_stream_multicast_addr_parse(const char *spec,
                                        struct sockaddr_storage *addr,
                                        socklen_t *addrlen)
{
    char buf[256];
    char *host;
    char *port_str;
    struct addrinfo hints, *result;
    int ret;

    if (!spec || !addr || !addrlen || *spec == '\0')
    {
        return -1;
    }

    if (strlen(spec) >= sizeof(buf))
    {
        return -1;
    }

    strcpy(buf, spec);

    /* Parse [IPv6]:port or IPv4:port */
    if (buf[0] == '[')
    {
        /* IPv6 bracket notation: [addr]:port */
        char *bracket = strchr(buf, ']');

        if (!bracket)
        {
            return -1;
        }

        *bracket = '\0';
        host = buf + 1;

        if (*(bracket + 1) != ':')
        {
            return -1;
        }

        port_str = bracket + 2;
    }
    else
    {
        /* IPv4: find last colon */
        char *colon = strrchr(buf, ':');

        if (!colon || colon == buf)
        {
            return -1;
        }

        *colon = '\0';
        host = buf;
        port_str = colon + 1;
    }

    if (*host == '\0' || *port_str == '\0')
    {
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

    ret = getaddrinfo(host, port_str, &hints, &result);

    if (ret != 0)
    {
        return -1;
    }

    /* Validate multicast range */
    if (result->ai_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)result->ai_addr;

        if (!IN_MULTICAST(ntohl(sin->sin_addr.s_addr)))
        {
            freeaddrinfo(result);
            return -1;
        }
    }
    else if (result->ai_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)result->ai_addr;

        if (!IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
        {
            freeaddrinfo(result);
            return -1;
        }
    }
    else
    {
        freeaddrinfo(result);
        return -1;
    }

    memcpy(addr, result->ai_addr, result->ai_addrlen);
    *addrlen = result->ai_addrlen;

    freeaddrinfo(result);
    return 0;
}


int rigctld_stream_multicast_socket_create(
    const struct sockaddr_storage *mcast_addr,
    socklen_t addrlen,
    int ttl,
    int *sock_fd,
    int *port)
{
    int fd;
    int reuse = 1;

    if (!mcast_addr || !sock_fd || !port)
    {
        return -1;
    }

    fd = socket(mcast_addr->ss_family, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, sizeof(reuse));

    /* Set multicast TTL / hop limit */
    if (mcast_addr->ss_family == AF_INET)
    {
        unsigned char ttl_val = (unsigned char)ttl;
        setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
                   (const char *)&ttl_val, sizeof(ttl_val));

        /* Bind to INADDR_ANY on the multicast port */
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = INADDR_ANY;
        bind_addr.sin_port = ((const struct sockaddr_in *)mcast_addr)->sin_port;

        if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
        {
            socket_close(fd);
            return -1;
        }

        *port = ntohs(bind_addr.sin_port);
    }
    else if (mcast_addr->ss_family == AF_INET6)
    {
        int hops = ttl;
        setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                   (const char *)&hops, sizeof(hops));

        /* Bind to in6addr_any on the multicast port */
        struct sockaddr_in6 bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin6_family = AF_INET6;
        bind_addr.sin6_addr = in6addr_any;
        bind_addr.sin6_port = ((const struct sockaddr_in6 *)mcast_addr)->sin6_port;

        if (bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0)
        {
            socket_close(fd);
            return -1;
        }

        *port = ntohs(bind_addr.sin6_port);
    }
    else
    {
        socket_close(fd);
        return -1;
    }

    (void)addrlen;
    *sock_fd = fd;
    return 0;
}


/* Compare two sockaddr_storage for matching address and port. */
static int sockaddr_equal(const struct sockaddr_storage *a,
                          const struct sockaddr_storage *b)
{
    if (a->ss_family != b->ss_family)
    {
        return 0;
    }

    if (a->ss_family == AF_INET)
    {
        const struct sockaddr_in *sa = (const struct sockaddr_in *)a;
        const struct sockaddr_in *sb = (const struct sockaddr_in *)b;
        return sa->sin_port == sb->sin_port
               && sa->sin_addr.s_addr == sb->sin_addr.s_addr;
    }

    if (a->ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *sa = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *sb = (const struct sockaddr_in6 *)b;
        return sa->sin6_port == sb->sin6_port
               && memcmp(&sa->sin6_addr, &sb->sin6_addr,
                         sizeof(sa->sin6_addr)) == 0;
    }

    return 0;
}


int rigctld_stream_registry_multicast_in_use(
    struct rigctld_stream_registry *reg,
    const struct sockaddr_storage *mcast_addr)
{
    int t, i;

    pthread_mutex_lock(&reg->lock);

    for (t = 0; t < RIG_STREAM_TYPE_COUNT; t++)
    {
        for (i = 0; i < RIGCTLD_MAX_STREAMS; i++)
        {
            struct rigctld_stream *s = reg->streams[t][i];

            if (s && s->multicast
                    && sockaddr_equal(&s->multicast_addr, mcast_addr))
            {
                pthread_mutex_unlock(&reg->lock);
                return 1;
            }
        }
    }

    pthread_mutex_unlock(&reg->lock);
    return 0;
}


/* --- Feeder thread implementation --- */


/* Wait for a subscribe packet from a client.
 * Records client address and sends back a subscribe ACK.
 * Returns 0 on success, -1 if stream stopped before subscribe arrived. */
static int wait_for_subscribe(struct rigctld_stream *stream)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    unsigned char buf[RIG_STREAM_MAX_DATAGRAM];
    struct rig_stream_packet_header hdr;
    time_t start_time = time(NULL);

    while (stream->running)
    {
        fd_set fds;
        struct timeval tv;

        if (time(NULL) - start_time >= stream->subscribe_timeout_s)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: subscribe timeout for stream %d\n",
                      __func__, stream->stream_id);
            return -1;
        }

        /* POSIX fd_set is a bitmask indexed by fd value; Winsock's is a
         * count-bounded handle array, where socket handles routinely exceed
         * FD_SETSIZE, so the value guard only applies off-Windows. */
#ifndef _WIN32

        if (stream->udp_sock >= FD_SETSIZE)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: fd %d >= FD_SETSIZE\n",
                      __func__, stream->udp_sock);
            return -1;
        }

#endif

        FD_ZERO(&fds);
        FD_SET(stream->udp_sock, &fds);
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(stream->udp_sock + 1, &fds, NULL, NULL, &tv);

        if (ready <= 0)
        {
            continue;  /* timeout or signal, retry if still running */
        }

        addr_len = sizeof(client_addr);
        ssize_t n = recvfrom(stream->udp_sock, (char *)buf, sizeof(buf), 0,
                             (struct sockaddr *)&client_addr, &addr_len);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        if (stream_packet_header_unpack(buf, (size_t)n, &hdr) < 0)
        {
            continue;
        }

        /* Validate token and source IP */
        if (validate_subscribe_packet(stream, &hdr, &client_addr) < 0)
        {
            continue;
        }

        /* Save client address for reply helpers */
        memcpy(&stream->client_addr, &client_addr, addr_len);
        stream->client_addr_len = addr_len;

        /* PING before subscribe — reply PONG, keep waiting */
        if ((hdr.control & RIG_STREAM_CTRL_PING)
                && hdr.stream_id == stream->stream_id)
        {
            rigctld_stream_send_control_reply(stream, RIG_STREAM_CTRL_PONG);
            continue;
        }

        if (!(hdr.control & RIG_STREAM_CTRL_SUBSCRIBE))
        {
            continue;
        }

        /* SUBSCRIBE received — finalize client address and send ACK */
        stream->client_addr_known = 1;
        stream->last_subscribe = time(NULL);
        rigctld_stream_send_control_reply(stream, RIG_STREAM_CTRL_SUBSCRIBE_ACK);

        return 0;
    }

    return -1;
}


/* Send a control reply (PONG, SUBSCRIBE_ACK, etc.) to the stream's client.
 * Returns 0 on success, -1 on failure. */
int rigctld_stream_send_control_reply(struct rigctld_stream *stream,
                                      uint16_t control_flag)
{
    unsigned char pkt[RIG_STREAM_HEADER_SIZE];
    struct rig_stream_packet_header hdr;

    stream_control_header_init(&hdr, (uint8_t)stream->type,
                               (uint16_t)stream->stream_id,
                               stream->subscribe_token, control_flag);
    hdr.source_id = stream->source_id;

    stream_packet_header_pack(&hdr, pkt);

    ssize_t sent = sendto(stream->udp_sock, (const char *)pkt,
                          RIG_STREAM_HEADER_SIZE, 0,
                          (struct sockaddr *)&stream->client_addr,
                          stream->client_addr_len);

    return (sent == RIG_STREAM_HEADER_SIZE) ? 0 : -1;
}


/* Auto-close a stream from within its feeder thread.
 * Marks the stream as dead and closes backend/socket resources.
 * The stream stays in the registry so stream_close or close_by_client
 * can find it, join the feeder thread, and free the struct.
 * The feeder must return NULL immediately after calling this. */
int rigctld_stream_auto_close(struct rigctld_stream_registry *reg,
                              struct rigctld_stream *stream)
{
    /* Close resources under registry lock so concurrent handlers
     * see auto_closed=1 before accessing backend_stream/udp_sock. */
    pthread_mutex_lock(&reg->lock);

    stream->auto_closed = 1;

    if (stream->backend_stream && stream->rig)
    {
        rig_stream_close(stream->rig, stream->backend_stream);
        stream->backend_stream = NULL;
    }

    if (stream->udp_sock >= 0)
    {
        socket_close(stream->udp_sock);
        stream->udp_sock = -1;
    }

    pthread_mutex_unlock(&reg->lock);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: auto-closed stream %d\n",
              __func__, stream->stream_id);

    return 0;
}


/* Populate a packet header with this stream's invariant fields plus the
 * per-packet timestamp, control bits and payload length, consuming one
 * sequence number. */
static void stream_fill_header(struct rigctld_stream *stream,
                               struct rig_stream_packet_header *hdr,
                               uint64_t timestamp, uint16_t control,
                               uint16_t payload_len)
{
    memset(hdr, 0, sizeof(*hdr));
    hdr->version = RIG_STREAM_PROTOCOL_VERSION;
    hdr->type = stream->type;
    hdr->stream_id = stream->stream_id;
    hdr->source_id = stream->source_id;
    hdr->subscribe_token = stream->subscribe_token;
    hdr->seq = stream->seq++;
    hdr->timestamp = timestamp;
    hdr->sample_rate = stream->config.sample_rate;
    hdr->format = stream->format_id;
    hdr->channels = stream->config.channels;
    hdr->control = control;
    hdr->payload_len = payload_len;
}


/* Send a packed packet to the subscribed client and update send accounting.
 * Returns 0 on success, -1 on send failure. */
static int send_to_client(struct rigctld_stream *stream,
                          const unsigned char *pkt, size_t len)
{
    ssize_t sent = sendto(stream->udp_sock, (const char *)pkt, len, 0,
                          (struct sockaddr *)&stream->client_addr,
                          stream->client_addr_len);

    if (sent >= 0)
    {
        stream->packet_count++;
        return 0;
    }

    stream->send_drops++;
    return -1;
}


/* Build and send a metadata packet for this stream.
 * Returns 0 on success, -1 on send failure. */
static int send_metadata_packet(struct rigctld_stream *stream,
                                const struct rig_stream_metadata *meta)
{
    unsigned char pkt[RIG_STREAM_HEADER_SIZE + RIG_STREAM_METADATA_WIRE_SIZE];
    struct rig_stream_packet_header hdr;

    stream_fill_header(stream, &hdr, stream->timestamp,
                       RIG_STREAM_CTRL_METADATA, RIG_STREAM_METADATA_WIRE_SIZE);

    stream_packet_header_pack(&hdr, pkt);
    stream_metadata_pack(meta, pkt + RIG_STREAM_HEADER_SIZE);

    if (send_to_client(stream, pkt, sizeof(pkt)) == 0)
    {
        stream->last_sent_meta = *meta;
        return 0;
    }

    return -1;
}


/* Build and send a time-only packet (RIG_STREAM_CTRL_TIME, 20-byte block,
 * no sample data) so time keeps advancing while the stream is idle.
 * Returns 0 when sent, -1 on send failure or when no usable time exists. */
static int send_time_only_packet(struct rigctld_stream *stream)
{
    struct rig_stream_read_info info;

    memset(&info, 0, sizeof(info));
    info.sample_index = rig_stream_get_samples_written(stream->backend_stream);
    stream_fill_read_time(stream->backend_stream, &info);

    if (!info.time_valid)
    {
        return -1;
    }

    unsigned char pkt[RIG_STREAM_HEADER_SIZE + RIG_STREAM_TIME_BLOCK_SIZE];
    struct rig_stream_packet_header hdr;

    stream_fill_header(stream, &hdr, info.sample_index,
                       RIG_STREAM_CTRL_TIME, RIG_STREAM_TIME_BLOCK_SIZE);

    struct rig_stream_time_anchor blk;
    memset(&blk, 0, sizeof(blk));
    blk.seconds = info.seconds;
    blk.picoseconds = info.picoseconds;
    blk.source = info.time_source;
    blk.flags = info.time_flags;
    blk.accuracy = info.time_accuracy;

    stream_packet_header_pack(&hdr, pkt);
    stream_time_block_pack(&blk, pkt + RIG_STREAM_HEADER_SIZE);

    return send_to_client(stream, pkt, sizeof(pkt));
}


/* Build and send an async write-status packet (RIG_STREAM_CTRL_WRITE_STATUS,
 * 36-byte block) reporting a TX under/overrun or late-burst event to the
 * client. sample_index rides the header timestamp. Returns 0 when sent. */
static int send_write_status_packet(struct rigctld_stream *stream,
                                    const struct rig_stream_write_status *ev)
{
    unsigned char pkt[RIG_STREAM_HEADER_SIZE + RIG_STREAM_WRITE_STATUS_WIRE_SIZE];
    struct rig_stream_packet_header hdr;

    stream_fill_header(stream, &hdr, ev->sample_index,
                       RIG_STREAM_CTRL_WRITE_STATUS,
                       RIG_STREAM_WRITE_STATUS_WIRE_SIZE);

    stream_packet_header_pack(&hdr, pkt);
    stream_write_status_pack(ev, pkt + RIG_STREAM_HEADER_SIZE);

    return send_to_client(stream, pkt, sizeof(pkt));
}


/* Forward one write-status event to the client (thin seam for testing). */
int rigctld_stream_emit_write_status(struct rigctld_stream *stream,
                                     const struct rig_stream_write_status *ev)
{
    return send_write_status_packet(stream, ev);
}


/* Drain the backend stream's write-status event FIFO and forward each event to
 * the client with its full per-event detail. */
static void poll_and_emit_write_status(struct rigctld_stream *stream)
{
    struct rig_stream_write_status ev;

    if (!stream->client_addr_known || !stream->backend_stream)
    {
        return;
    }

    while (rig_stream_wait_write_status(stream->rig, stream->backend_stream,
                                        &ev, 0) == RIG_OK)
    {
        send_write_status_packet(stream, &ev);
    }
}


/* Elapsed milliseconds between two timespecs. */
static long elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    long result = (end->tv_sec - start->tv_sec) * 1000
                  + (end->tv_nsec - start->tv_nsec) / 1000000;
    return result < 0 ? 0 : result;
}


int rigctld_stream_metadata_refresh_due(int refresh_ms, int sample_rate,
                                        uint64_t samples_since)
{
    if (refresh_ms == 0)
    {
        return samples_since > 0;
    }

    if (sample_rate <= 0)
    {
        return 0;
    }

    /* samples_since / sample_rate * 1000 >= refresh_ms, without float */
    return samples_since * 1000
           >= (uint64_t)refresh_ms * (uint64_t)sample_rate;
}


/* TX feeder thread: receives UDP packets from client, writes to backend ring buffer.
 * Records client address from first packet. Detects sequence gaps. */
static void *rigctld_stream_feeder_tx(void *arg)
{
    struct rigctld_stream *stream = (struct rigctld_stream *)arg;

#ifdef __APPLE__
    {
        char tname[16];
        snprintf(tname, sizeof(tname), "hl-tx-%d", stream->stream_id);
        pthread_setname_np(tname);
    }
#endif

    unsigned char pkt_buf[RIG_STREAM_MAX_DATAGRAM];
    int first_data = 1;  /* Skip gap detection on first data packet */
    struct timespec ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_now);
    time_t last_activity = ts_now.tv_sec;

    /* Cache per-stream invariants */
    stream->format_id = stream_format_to_id(stream->config.format);
    stream->sample_size = rig_stream_format_sample_size(stream->config.format);

    if (stream->sample_size <= 0)
    {
        stream->sample_size = 1;
    }

    int sample_size = stream->sample_size;
    int channels = stream->config.channels > 0 ? stream->config.channels : 1;
    int frame_bytes = sample_size * channels;

    while (stream->running)
    {
        /* select() with 100ms timeout for graceful shutdown */
        fd_set fds;
        struct timeval tv;

        /* See the RX feeder: the FD_SETSIZE value guard applies only to POSIX
         * bitmask fd_set, not Winsock's count-bounded handle array. */
#ifndef _WIN32

        if (stream->udp_sock >= FD_SETSIZE)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: fd %d >= FD_SETSIZE\n",
                      __func__, stream->udp_sock);
            break;
        }

#endif

        FD_ZERO(&fds);
        FD_SET(stream->udp_sock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ready = select(stream->udp_sock + 1, &fds, NULL, NULL, &tv);

        if (ready <= 0)
        {
            /* Backend write-status events (e.g. a late burst) are recorded
             * asynchronously by the backend's TX thread, so drain them on the
             * idle path too — not only when a client packet arrives. */
            poll_and_emit_write_status(stream);

            /* Check inactivity timeout */
            clock_gettime(CLOCK_MONOTONIC, &ts_now);

            if (ts_now.tv_sec - last_activity >= stream->subscribe_timeout_s)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s: inactivity timeout for TX stream %d\n",
                          __func__, stream->stream_id);
                stream->running = 0;
                rigctld_stream_auto_close(&g_stream_registry, stream);
                return NULL;
            }

            continue;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        last_activity = ts_now.tv_sec;

        struct sockaddr_storage from_addr;
        socklen_t from_len = sizeof(from_addr);
        ssize_t n = recvfrom(stream->udp_sock, (char *)pkt_buf,
                             sizeof(pkt_buf), 0,
                             (struct sockaddr *)&from_addr, &from_len);

        if (n < RIG_STREAM_HEADER_SIZE)
        {
            continue;
        }

        /* Validate header */
        struct rig_stream_packet_header hdr;

        if (stream_packet_header_unpack(pkt_buf, (size_t)n, &hdr) < 0)
        {
            continue;
        }

        /* Reject frames whose claimed payload exceeds the bytes received,
         * so payload reads (metadata or data) stay within the datagram. */
        if (hdr.payload_len > (uint16_t)(n - RIG_STREAM_HEADER_SIZE))
        {
            rig_debug(RIG_DEBUG_WARN,
                      "Stream %d: payload_len %u exceeds %zd received body bytes\n",
                      stream->stream_id, hdr.payload_len,
                      (ssize_t)(n - RIG_STREAM_HEADER_SIZE));
            continue;
        }

        /* Validate token and source IP */
        if (validate_subscribe_packet(stream, &hdr, &from_addr) < 0)
        {
            continue;
        }

        /* Record client address on first valid packet */
        if (!stream->client_addr_known)
        {
            memcpy(&stream->client_addr, &from_addr, from_len);
            stream->client_addr_len = from_len;
            stream->client_addr_known = 1;
        }

        /* PING — reply with PONG, don't process as data */
        if ((hdr.control & RIG_STREAM_CTRL_PING)
                && hdr.stream_id == stream->stream_id)
        {
            rigctld_stream_send_control_reply(stream, RIG_STREAM_CTRL_PONG);
            continue;
        }

        /* Metadata frame — apply immediately */
        if (hdr.control & RIG_STREAM_CTRL_METADATA)
        {
            if (hdr.payload_len >= RIG_STREAM_METADATA_WIRE_SIZE)
            {
                struct rig_stream_metadata meta;
                stream_metadata_unpack(pkt_buf + RIG_STREAM_HEADER_SIZE,
                                       hdr.payload_len, &meta);
                rig_stream_write_metadata(stream->rig,
                                          stream->backend_stream, &meta);
            }

            stream->packet_count++;
            continue;
        }

        /* Data frame — sequence gap detection.
         * Skip gap check on first data packet (client may start at any seq). */
        if (!first_data && hdr.seq != stream->expected_seq)
        {
            uint32_t gap = hdr.seq - stream->expected_seq;

            if (gap >= 0x80000000u)
            {
                /* Backward delta: reordering or wrap, not a forward loss. */
            }
            else if (gap > RIGCTLD_MAX_PLAUSIBLE_SEQ_GAP)
            {
                /* An implausibly large forward jump (e.g. a desynced or
                 * spoofed seq) is counted as one discontinuity rather than
                 * adding a huge bogus loss total. */
                stream->gap_count += 1;
                rig_debug(RIG_DEBUG_WARN,
                          "Stream %d: implausible seq jump %u->%u; "
                          "counted as one gap\n",
                          stream->stream_id, stream->expected_seq, hdr.seq);
            }
            else
            {
                stream->gap_count += gap;
                rig_debug(RIG_DEBUG_WARN,
                          "Stream %d: %u TX packets lost (seq %u->%u)\n",
                          stream->stream_id, gap,
                          stream->expected_seq, hdr.seq);
            }
        }

        first_data = 0;
        stream->expected_seq = hdr.seq + 1;

        /* Write payload to backend ring buffer */
        size_t payload_len = (size_t)n - RIG_STREAM_HEADER_SIZE;

        if (payload_len > 0 && payload_len == hdr.payload_len)
        {
            const unsigned char *data = pkt_buf + RIG_STREAM_HEADER_SIZE;
            size_t data_len = payload_len;
            struct rig_stream_write_info winfo;
            const struct rig_stream_write_info *winfo_ptr = NULL;

            /* Embedded TX burst target: the payload begins with a time
             * block carrying the target instant and SOB/EOB flags. */
            if (hdr.control & RIG_STREAM_CTRL_TIME)
            {
                struct rig_stream_time_anchor blk;

                if (!stream_ctrl_time_valid(hdr.control)
                        || stream_time_block_unpack(data, data_len, &blk) != 0)
                {
                    rig_debug(RIG_DEBUG_WARN,
                              "Stream %d: dropping malformed TIME packet\n",
                              stream->stream_id);
                    continue;
                }

                memset(&winfo, 0, sizeof(winfo));
                winfo.time_valid = (blk.flags & RIG_STREAM_TIME_FLAG_TX_TIMED)
                                   ? 1 : 0;
                winfo.seconds = blk.seconds;
                winfo.picoseconds = blk.picoseconds;
                winfo.flags = blk.flags & (RIG_STREAM_TIME_FLAG_SOB
                                           | RIG_STREAM_TIME_FLAG_EOB);
                winfo_ptr = &winfo;

                data += RIG_STREAM_TIME_BLOCK_SIZE;
                data_len -= RIG_STREAM_TIME_BLOCK_SIZE;
            }

            if (data_len > 0 || winfo_ptr)
            {
                size_t bytes_written;
                rig_stream_write(stream->rig, stream->backend_stream,
                                 data, data_len, &bytes_written, 0,
                                 winfo_ptr);

                /* Advance the sample counter by whole frames (all
                 * channels); codec frame durations are not on the wire,
                 * so a codec TX position stays byte-agnostic (0). */
                if (!stream->backend_stream->is_codec)
                {
                    stream->timestamp += data_len / frame_bytes;
                }
            }
        }

        /* Forward any write-status events the backend recorded (late burst,
         * under/overrun) to the client. */
        poll_and_emit_write_status(stream);

        stream->packet_count++;
    }

    return NULL;
}


/* Poll for a pending client control packet (PING or re-SUBSCRIBE) and reply.
 * Reads only when a datagram is waiting so the feeder never blocks. */
static void poll_incoming_control(struct rigctld_stream *stream,
                                  const struct timespec *now)
{
    unsigned char ctrl_buf[RIG_STREAM_MAX_DATAGRAM];
    struct sockaddr_storage from_addr;
    socklen_t from_len = sizeof(from_addr);

    /* Read only when a datagram is pending: MSG_DONTWAIT is a no-op on
     * Winsock (defined to 0), so a bare recvfrom would block the TX
     * feeder until a client packet arrived. A zero-timeout select()
     * gates the read portably. */
    ssize_t ctrl_n = -1;
    fd_set ctrl_fds;
    struct timeval ctrl_tv = { 0, 0 };
    FD_ZERO(&ctrl_fds);
#ifndef _WIN32

    if (stream->udp_sock < FD_SETSIZE)
#endif
    {
        FD_SET(stream->udp_sock, &ctrl_fds);

        if (select(stream->udp_sock + 1, &ctrl_fds, NULL, NULL,
                   &ctrl_tv) > 0)
        {
            ctrl_n = recvfrom(stream->udp_sock,
                              (char *)ctrl_buf, sizeof(ctrl_buf),
                              MSG_DONTWAIT,
                              (struct sockaddr *)&from_addr,
                              &from_len);
        }
    }

    if (ctrl_n < RIG_STREAM_HEADER_SIZE)
    {
        return;
    }

    struct rig_stream_packet_header ctrl_hdr;

    if (stream_packet_header_unpack(ctrl_buf, (size_t)ctrl_n, &ctrl_hdr) != 0
            || validate_subscribe_packet(stream, &ctrl_hdr, &from_addr) != 0)
    {
        return;
    }

    if ((ctrl_hdr.control & RIG_STREAM_CTRL_PING)
            && ctrl_hdr.stream_id == stream->stream_id)
    {
        rigctld_stream_send_control_reply(stream, RIG_STREAM_CTRL_PONG);
        stream->last_subscribe = now->tv_sec;
    }
    else if (ctrl_hdr.control & RIG_STREAM_CTRL_SUBSCRIBE)
    {
        /* Re-subscribe: update client address, send ACK */
        memcpy(&stream->client_addr, &from_addr, from_len);
        stream->client_addr_len = from_len;
        stream->last_subscribe = now->tv_sec;
        rigctld_stream_send_control_reply(stream, RIG_STREAM_CTRL_SUBSCRIBE_ACK);
    }
}


/* RX feeder thread: reads from backend ring buffer, sends UDP packets.
 * Sends initial metadata after subscribe, then polls for changes. */
static void *rigctld_stream_feeder_rx(void *arg)
{
    struct rigctld_stream *stream = (struct rigctld_stream *)arg;

#ifdef __APPLE__
    {
        char tname[16];
        snprintf(tname, sizeof(tname), "hl-rx-%d", stream->stream_id);
        pthread_setname_np(tname);
    }
#endif

    /* Cache per-stream invariants */
    stream->format_id = stream_format_to_id(stream->config.format);
    stream->sample_size = rig_stream_format_sample_size(stream->config.format);

    if (stream->sample_size <= 0)
    {
        stream->sample_size = 1;
    }

    int sample_size = stream->sample_size;
    int channels = stream->config.channels > 0 ? stream->config.channels : 1;
    int frame_bytes = sample_size * channels;

    /* Derive the per-datagram budget from the configured (clamped) MTU, then
     * reserve room for the embedded time block so stamped packets never
     * exceed it. */
    int mtu_payload = stream_max_payload_from_mtu(stream->config.mtu, 0);
    int max_payload = mtu_payload - RIG_STREAM_TIME_BLOCK_SIZE;
    /* Align to frame boundary */
    max_payload = (max_payload / frame_bytes) * frame_bytes;

    /* Template header with invariant fields (copy + patch per packet) */
    struct rig_stream_packet_header hdr_template;
    memset(&hdr_template, 0, sizeof(hdr_template));
    hdr_template.version = RIG_STREAM_PROTOCOL_VERSION;
    hdr_template.type = stream->type;
    hdr_template.stream_id = stream->stream_id;
    hdr_template.source_id = stream->source_id;
    hdr_template.subscribe_token = stream->subscribe_token;
    hdr_template.sample_rate = stream->config.sample_rate;
    hdr_template.format = stream->format_id;
    hdr_template.channels = stream->config.channels;

    /* Layout: [header][time block][data]. A stamped packet sends from
     * pkt_buf; an unstamped one packs its header into the block slot and
     * sends from pkt_buf + RIG_STREAM_TIME_BLOCK_SIZE — zero-copy both
     * ways. */
    unsigned char *pkt_buf = malloc(RIG_STREAM_HEADER_SIZE
                                    + RIG_STREAM_TIME_BLOCK_SIZE
                                    + max_payload);

    if (!pkt_buf)
    {
        return NULL;
    }

    unsigned char *payload = pkt_buf + RIG_STREAM_HEADER_SIZE
                             + RIG_STREAM_TIME_BLOCK_SIZE;

    /* Multicast: send directly to group, no subscribe handshake needed.
     * Unicast: wait for client subscribe packet. */
    if (stream->multicast)
    {
        stream->client_addr = stream->multicast_addr;
        stream->client_addr_len = stream->multicast_addr_len;
        stream->client_addr_known = 1;
    }
    else if (wait_for_subscribe(stream) < 0)
    {
        free(pkt_buf);
        stream->running = 0;
        rigctld_stream_auto_close(&g_stream_registry, stream);
        return NULL;
    }

    /* Seed monotonic timestamps for interval checks */
    struct timespec last_meta_time, last_ctrl_check, last_data_time;
    clock_gettime(CLOCK_MONOTONIC, &last_meta_time);
    last_ctrl_check = last_meta_time;
    last_data_time = last_meta_time;
    stream->last_subscribe = last_meta_time.tv_sec;

    /* Producer sample index at the last metadata frame — drives the
     * unconditional refresh cadence. */
    uint64_t last_meta_sample = 0;

    {
        struct rig_stream_metadata meta;

        if (rig_stream_read_metadata(stream->rig,
                                     stream->backend_stream,
                                     &meta) == RIG_OK)
        {
            send_metadata_packet(stream, &meta);
            clock_gettime(CLOCK_MONOTONIC, &last_meta_time);
        }
    }

    /* Main feeder loop */
    while (stream->running)
    {
        size_t bytes_read = 0;
        struct rig_stream_read_info rinfo;
        int ret = rig_stream_read(stream->rig, stream->backend_stream,
                                  payload, max_payload, &bytes_read, 100,
                                  &rinfo);

        if (ret == RIG_OK && bytes_read > 0)
        {
            /* Patch per-packet fields into template header. The header
             * timestamp carries the producer sample index, so upstream
             * holes appear as timestamp jumps on the wire. */
            struct rig_stream_packet_header hdr = hdr_template;
            hdr.seq = stream->seq++;
            hdr.timestamp = rinfo.sample_index;
            hdr.control = 0;

            unsigned char *send_ptr;
            size_t send_len;

            /* Stamp every packet that has usable time, and — MUST-stamp
             * rule — every packet whose timestamp jumps (discontinuity). */
            if (rinfo.time_valid
                    || (rinfo.time_flags & RIG_STREAM_TIME_FLAG_DISCONTINUITY))
            {
                struct rig_stream_time_anchor blk;
                memset(&blk, 0, sizeof(blk));
                blk.seconds = rinfo.seconds;
                blk.picoseconds = rinfo.picoseconds;
                blk.source = rinfo.time_source;
                blk.flags = rinfo.time_flags;
                blk.accuracy = rinfo.time_accuracy;

                hdr.control |= RIG_STREAM_CTRL_TIME;
                hdr.payload_len = (uint16_t)(RIG_STREAM_TIME_BLOCK_SIZE
                                             + bytes_read);
                stream_packet_header_pack(&hdr, pkt_buf);
                stream_time_block_pack(&blk,
                                       pkt_buf + RIG_STREAM_HEADER_SIZE);
                send_ptr = pkt_buf;
                send_len = RIG_STREAM_HEADER_SIZE
                           + RIG_STREAM_TIME_BLOCK_SIZE + bytes_read;
            }
            else
            {
                hdr.payload_len = (uint16_t)bytes_read;
                send_ptr = pkt_buf + RIG_STREAM_TIME_BLOCK_SIZE;
                stream_packet_header_pack(&hdr, send_ptr);
                send_len = RIG_STREAM_HEADER_SIZE + bytes_read;
            }

            send_to_client(stream, send_ptr, send_len);

            /* Track the producer position for non-data frames. A codec
             * stream advances by the frame's decoded duration, not by
             * bytes. */
            stream->timestamp = stream->backend_stream->is_codec
                                ? rinfo.sample_index
                                + rinfo.codec_frame_samples
                                : rinfo.sample_index
                                + bytes_read / (size_t)frame_bytes;

            /* Unconditional metadata refresh (cadence in stream-data time)
             * so a lost change-detected frame heals without waiting for the
             * next change; interval 0 sends metadata with every data packet. */
            if (rigctld_stream_metadata_refresh_due(stream->metadata_refresh_ms,
                                                    stream->config.sample_rate,
                                                    stream->timestamp - last_meta_sample))
            {
                struct rig_stream_metadata meta;

                if (rig_stream_read_metadata(stream->rig,
                                             stream->backend_stream,
                                             &meta) == RIG_OK)
                {
                    send_metadata_packet(stream, &meta);
                }

                last_meta_sample = stream->timestamp;
            }
        }

        /* Single clock_gettime per iteration for all interval checks (vDSO) */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (ret == RIG_OK && bytes_read > 0)
        {
            last_data_time = now;
        }

        /* Check for incoming client packets (PING, re-SUBSCRIBE) — unicast only.
         * Time-gated to avoid a syscall per iteration at high packet rates. */
        if (!stream->multicast && elapsed_ms(&last_ctrl_check, &now) >= 50)
        {
            poll_incoming_control(stream, &now);
            last_ctrl_check = now;
        }

        /* Keepalive timeout check — unicast only */
        if (!stream->multicast
                && now.tv_sec - stream->last_subscribe
                >= stream->subscribe_timeout_s)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: keepalive timeout for stream %d\n",
                      __func__, stream->stream_id);
            free(pkt_buf);
            stream->running = 0;
            rigctld_stream_auto_close(&g_stream_registry, stream);
            return NULL;
        }

        /* Metadata polling: check at configured interval */
        if (stream->metadata_interval_ms > 0)
        {
            if (elapsed_ms(&last_meta_time, &now)
                    >= stream->metadata_interval_ms)
            {
                struct rig_stream_metadata meta;

                if (rig_stream_read_metadata(stream->rig,
                                             stream->backend_stream,
                                             &meta) == RIG_OK)
                {
                    if (stream_metadata_changed(&meta, &stream->last_sent_meta))
                    {
                        send_metadata_packet(stream, &meta);
                        /* A frame just went out; restart the refresh cadence. */
                        last_meta_sample = stream->timestamp;
                    }
                }

                /* Idle heartbeat: when no data packet carried a time block
                 * since the last interval, keep time advancing with a
                 * time-only packet. */
                if (elapsed_ms(&last_data_time, &now)
                        >= stream->metadata_interval_ms)
                {
                    send_time_only_packet(stream);
                }

                clock_gettime(CLOCK_MONOTONIC, &last_meta_time);
            }
        }
    }

    free(pkt_buf);
    return NULL;
}


int rigctld_stream_feeder_start(struct rigctld_stream *stream)
{
    if (!stream)
    {
        return -1;
    }

    int is_rx = stream_type_is_rx(stream->type);
    int is_tx = stream_type_is_tx(stream->type);

    if (!is_rx && !is_tx)
    {
        return 0;
    }

    stream->running = 1;
    stream->auto_closed = 0;
    stream->seq = 0;
    stream->expected_seq = 0;
    stream->timestamp = 0;
    stream->packet_count = 0;
    stream->gap_count = 0;
    stream->send_drops = 0;
    stream->client_addr_known = 0;

    /* Clamp metadata interval to valid range (RX only — TX doesn't poll) */
    if (is_rx)
    {
        if (stream->metadata_interval_ms == 0)
        {
            stream->metadata_interval_ms = RIGCTLD_METADATA_INTERVAL_DEFAULT;
        }

        if (stream->metadata_interval_ms < RIGCTLD_METADATA_INTERVAL_MIN)
        {
            stream->metadata_interval_ms = RIGCTLD_METADATA_INTERVAL_MIN;
        }

        if (stream->metadata_interval_ms > RIGCTLD_METADATA_INTERVAL_MAX)
        {
            stream->metadata_interval_ms = RIGCTLD_METADATA_INTERVAL_MAX;
        }
    }

    void *(*feeder_fn)(void *) = is_rx ? rigctld_stream_feeder_rx :
                                 rigctld_stream_feeder_tx;
    int err = pthread_create(&stream->feeder_thread, NULL, feeder_fn, stream);

    if (err != 0)
    {
        stream->running = 0;
        return -1;
    }

    stream->feeder_started = 1;

#ifdef __linux__
    {
        char tname[16];
        snprintf(tname, sizeof(tname), "hl-%s-%d",
                 stream_type_is_rx(stream->type) ? "rx" : "tx",
                 stream->stream_id);
        pthread_setname_np(stream->feeder_thread, tname);
    }
#endif

    return 0;
}


int rigctld_stream_feeder_stop(struct rigctld_stream *stream)
{
    if (!stream)
    {
        return -1;
    }

    if (!stream->feeder_started)
    {
        return 0;
    }

    /* Signal the thread to stop */
    stream->running = 0;

    /* Always join: the feeder thread is never detached, so it is always
     * joinable.  If auto_closed, the thread has already exited and join
     * returns immediately. */
    pthread_join(stream->feeder_thread, NULL);
    stream->feeder_started = 0;

    return 0;
}
