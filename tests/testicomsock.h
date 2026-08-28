/*
 * Socket-pair plumbing shared by the Icom CI-V tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * A test drives a backend over one end of a local socket connection
 * while a fake rig answers on the other.  On POSIX that is a socket
 * pair; on Windows, where socketpair() does not exist, a loopback TCP
 * connection stands in for it.
 */

#ifndef TESTICOMSOCK_H
#define TESTICOMSOCK_H

#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

int close_test_socket(int fd);
int read_test_socket(int fd, void *buffer, size_t length);
int write_test_socket(int fd, const void *buffer, size_t length);
int open_test_connection(int sockets[2]);

/* Reads one CI-V frame, i.e. everything up to and including an FD byte */
int read_frame(int fd, unsigned char *buffer, size_t capacity,
               size_t *length);
int write_all(int fd, const unsigned char *buffer, size_t length);

#endif /* TESTICOMSOCK_H */
