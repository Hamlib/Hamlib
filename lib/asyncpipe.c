#include <hamlib/config.h>

#include "asyncpipe.h"

#if defined(WIN32) && defined(HAVE_WINDOWS_H)

static volatile long pipe_serial_nunber;

int async_pipe_create(hamlib_async_pipe_t **pipe_out,
                      unsigned long pipe_buffer_size, unsigned long pipe_connect_timeout_millis)
{
    DWORD error_code;
    CHAR pipe_name[MAX_PATH];
    hamlib_async_pipe_t *pipe;

    pipe = calloc(1, sizeof(hamlib_async_pipe_t));

    if (pipe == NULL)
    {
        return -RIG_ENOMEM;
    }

    if (pipe_buffer_size == 0)
    {
        pipe_buffer_size = PIPE_BUFFER_SIZE_DEFAULT;
    }

    SNPRINTF(pipe_name, sizeof(pipe_name),
             "\\\\.\\Pipe\\Hamlib.%08lx.%08lx",
             GetCurrentProcessId(),
             InterlockedIncrement(&pipe_serial_nunber)
            );

    pipe->read = CreateNamedPipe(
                     pipe_name,
                     PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                     PIPE_TYPE_BYTE | PIPE_WAIT,
                     1,                           // Number of pipes
                     pipe_buffer_size,            // Out buffer size
                     pipe_buffer_size,            // In buffer size
                     pipe_connect_timeout_millis, // Timeout in ms
                     NULL);

    if (!pipe->read)
    {
        return -RIG_EINTERNAL;
    }

    pipe->write = CreateFile(
                      pipe_name,
                      GENERIC_WRITE,
                      0, // No sharing
                      NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                      NULL // Template file
                  );

    if (pipe->write == INVALID_HANDLE_VALUE)
    {
        error_code = GetLastError();
        CloseHandle(pipe->read);
        free(pipe);
        SetLastError(error_code);
        return -RIG_EINTERNAL;
    }

    pipe->read_overlapped.hEvent = CreateEvent(
                                       NULL,    // default security attribute
                                       TRUE,    // manual-reset event
                                       FALSE,   // initial state = not signaled
                                       NULL);   // unnamed event object

    if (pipe->read_overlapped.hEvent == NULL)
    {
        error_code = GetLastError();
        CloseHandle(pipe->read);
        CloseHandle(pipe->write);
        free(pipe);
        SetLastError(error_code);
        return -RIG_EINTERNAL;
    }

    pipe->write_overlapped.hEvent = CreateEvent(
                                        NULL,    // default security attribute
                                        TRUE,    // manual-reset event
                                        FALSE,   // initial state = not signaled
                                        NULL);   // unnamed event object

    if (pipe->write_overlapped.hEvent == NULL)
    {
        error_code = GetLastError();
        CloseHandle(pipe->read_overlapped.hEvent);
        CloseHandle(pipe->read);
        CloseHandle(pipe->write);
        free(pipe);
        SetLastError(error_code);
        return -RIG_EINTERNAL;
    }

    *pipe_out = pipe;

    return RIG_OK;
}

void async_pipe_close(hamlib_async_pipe_t *pipe)
{
    if (pipe->read != NULL)
    {
        CloseHandle(pipe->read);
        pipe->read = NULL;
    }

    if (pipe->write != NULL)
    {
        CloseHandle(pipe->write);
        pipe->write = NULL;
    }

    if (pipe->read_overlapped.hEvent != NULL)
    {
        CloseHandle(pipe->read_overlapped.hEvent);
        pipe->read_overlapped.hEvent = NULL;
    }

    if (pipe->write_overlapped.hEvent != NULL)
    {
        CloseHandle(pipe->write_overlapped.hEvent);
        pipe->write_overlapped.hEvent = NULL;
    }

    free(pipe);
}

ssize_t async_pipe_read(hamlib_async_pipe_t *pipe, void *buf, size_t count,
                        int timeout)
{
    HANDLE event_handles[1] =
    {
        pipe->read_overlapped.hEvent,
    };
    HANDLE read_handle = pipe->read;
    LPOVERLAPPED overlapped = &pipe->read_overlapped;
    DWORD wait_result;
    int result;
    ssize_t bytes_read;

    result = ReadFile(read_handle, buf, count, NULL, overlapped);

    if (!result)
    {
        result = GetLastError();

        switch (result)
        {
        case ERROR_SUCCESS:
            // No error?
            break;

        case ERROR_IO_PENDING:
            wait_result = WaitForMultipleObjects(1, event_handles, FALSE, timeout);

            switch (wait_result)
            {
            case WAIT_OBJECT_0 + 0:
                break;

            case WAIT_TIMEOUT:
                if (count == 0)
                {
                    // Zero-length reads are used to wait for incoming data,
                    // so the I/O operation needs to be cancelled in case of a timeout
                    CancelIo(read_handle);
                    return -RIG_ETIMEOUT;
                }
                else
                {
                    // Should not happen, as reads with count > 0 are used only when there is data available in the pipe
                    return -RIG_EINTERNAL;
                }

            default:
                result = GetLastError();
                rig_debug(RIG_DEBUG_ERR, "%s: WaitForMultipleObjects() error: %d\n", __func__,
                          result);
                return -RIG_EINTERNAL;
            }

            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: ReadFile() error: %d\n", __func__, result);
            return -RIG_EIO;
        }
    }

    result = GetOverlappedResult(read_handle, overlapped, (LPDWORD) &bytes_read,
                                 FALSE);

    if (!result)
    {
        result = GetLastError();

        switch (result)
        {
        case ERROR_SUCCESS:
            // No error?
            break;

        case ERROR_IO_PENDING:
            // Shouldn't happen?
            return -RIG_ETIMEOUT;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: GetOverlappedResult() error: %d\n", __func__,
                      result);
            return -RIG_EIO;
        }
    }

    return bytes_read;
}

int async_pipe_wait_for_data(hamlib_async_pipe_t *pipe, int timeout)
{
    unsigned char data;
    int result;

    // Use a zero-length read to wait for data in pipe
    result = async_pipe_read(pipe, &data, 0, timeout);

    if (result > 0)
    {
        return RIG_OK;
    }

    return result;
}

ssize_t async_pipe_write(hamlib_async_pipe_t *pipe, const unsigned char *buf,
                         size_t count, int timeout)
{
    HANDLE event_handles[1] =
    {
        pipe->write_overlapped.hEvent,
    };
    HANDLE write_handle = pipe->write;
    LPOVERLAPPED overlapped = &pipe->write_overlapped;
    DWORD wait_result;
    int result;
    ssize_t bytes_written;

    result = WriteFile(write_handle, buf, count, NULL, overlapped);

    if (!result)
    {
        result = GetLastError();

        switch (result)
        {
        case ERROR_SUCCESS:
            // No error?
            break;

        case ERROR_IO_PENDING:
            wait_result = WaitForMultipleObjects(1, event_handles, FALSE, timeout);

            switch (wait_result)
            {
            case WAIT_OBJECT_0 + 0:
                break;

            case WAIT_TIMEOUT:
                CancelIo(write_handle);
                return -RIG_ETIMEOUT;

            default:
                result = GetLastError();
                rig_debug(RIG_DEBUG_ERR, "%s: WaitForMultipleObjects() error: %d\n", __func__,
                          result);
                return -RIG_EINTERNAL;
            }

            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: WriteFile() error: %d\n", __func__, result);
            return -RIG_EIO;
        }
    }

    result = GetOverlappedResult(write_handle, overlapped, (LPDWORD) &bytes_written,
                                 FALSE);

    if (!result)
    {
        result = GetLastError();

        switch (result)
        {
        case ERROR_SUCCESS:
            // No error?
            break;

        case ERROR_IO_PENDING:
            // Shouldn't happen?
            return -RIG_ETIMEOUT;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: GetOverlappedResult() error: %d\n", __func__,
                      result);
            return -RIG_EIO;
        }
    }

    return bytes_written;
}

#endif
