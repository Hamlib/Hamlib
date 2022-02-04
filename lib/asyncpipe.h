#ifndef _ASYNC_PIPE_H
#define _ASYNC_PIPE_H 1

#include "hamlib/config.h"

#if defined(WIN32) && defined(HAVE_WINDOWS_H)

#include <hamlib/rig.h>
#include <windows.h>

#define PIPE_BUFFER_SIZE_DEFAULT 65536

struct hamlib_async_pipe {
    HANDLE write;
    HANDLE read;
    OVERLAPPED write_overlapped;
    OVERLAPPED read_overlapped;
};

int async_pipe_create(hamlib_async_pipe_t **pipe_out, unsigned long pipe_buffer_size, unsigned long pipe_connect_timeout_millis);
void async_pipe_close(hamlib_async_pipe_t *pipe);
ssize_t async_pipe_read(hamlib_async_pipe_t *pipe, void *buf, size_t count, int timeout);
int async_pipe_wait_for_data(hamlib_async_pipe_t *pipe, int timeout);
ssize_t async_pipe_write(hamlib_async_pipe_t *pipe, const unsigned char *buf, size_t count, int timeout);

#endif

#endif
