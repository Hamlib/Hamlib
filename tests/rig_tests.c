#include <stdio.h>
#include <string.h>
#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"

#define REDACTED_ARGUMENT "<redacted>"

static int append_startup_text(char *buffer, size_t buffer_size,
                               size_t *length, const char *text)
{
    size_t text_length = strlen(text);
    size_t available;

    if (*length >= buffer_size)
    {
        return -RIG_ETRUNC;
    }

    available = buffer_size - *length - 1;

    if (text_length > available)
    {
        memcpy(buffer + *length, text, available);
        *length += available;
        buffer[*length] = '\0';
        return -RIG_ETRUNC;
    }

    memcpy(buffer + *length, text, text_length + 1);
    *length += text_length;
    return RIG_OK;
}

static int is_password_command(const char *argument)
{
    return strcmp(argument, "password") == 0
           || strcmp(argument, "\\password") == 0
           || strcmp(argument, "+\\password") == 0
           || (argument[0] == (char)0x98 && argument[1] == '\0');
}

int rigctl_format_startup_args(char *buffer, size_t buffer_size,
                               const char *prefix, int argc,
                               char *const argv[])
{
    size_t length = 0;
    int redact_next = 0;
    int result = RIG_OK;
    int i;

    if (buffer == NULL || buffer_size == 0 || prefix == NULL || argc < 0
            || (argc > 0 && argv == NULL))
    {
        return -RIG_EINVAL;
    }

    buffer[0] = '\0';

    if (append_startup_text(buffer, buffer_size, &length, prefix) != RIG_OK)
    {
        result = -RIG_ETRUNC;
    }

    for (i = 0; i < argc; ++i)
    {
        const char *argument = argv[i] == NULL ? "" : argv[i];
        const char *display_argument = argument;
        char redacted_option[sizeof("--password=" REDACTED_ARGUMENT)];

        if (redact_next)
        {
            display_argument = REDACTED_ARGUMENT;
            redact_next = 0;
        }
        else if (strcmp(argument, "-A") == 0
                 || strcmp(argument, "--password") == 0
                 || is_password_command(argument))
        {
            redact_next = 1;
        }
        else if (strncmp(argument, "-A", 2) == 0 && argument[2] != '\0')
        {
            snprintf(redacted_option, sizeof(redacted_option), "-A%s",
                     REDACTED_ARGUMENT);
            display_argument = redacted_option;
        }
        else if (strncmp(argument, "--password=", 11) == 0)
        {
            snprintf(redacted_option, sizeof(redacted_option),
                     "--password=%s", REDACTED_ARGUMENT);
            display_argument = redacted_option;
        }

        if (append_startup_text(buffer, buffer_size, &length, " ") != RIG_OK
                || append_startup_text(buffer, buffer_size, &length,
                                       display_argument) != RIG_OK)
        {
            result = -RIG_ETRUNC;
        }
    }

    return result;
}

void rigctl_wipe_password(char *password)
{
    volatile unsigned char *cursor = (volatile unsigned char *)password;
    size_t length;

    if (cursor == NULL)
    {
        return;
    }

    length = strlen(password);

    while (length-- > 0)
    {
        *cursor++ = '*';
    }
}


// cppcheck-suppress unusedFunction
int rig_test_cw(RIG *rig)
{
    char *s = "SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS SOS";
    //char *s = "TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST";
    int i;
    ELAPSED1;
    ENTERFUNC;

    for (i = 0; i < strlen(s); ++i)
    {
        char cw[2];
        cw[0] = s[i];
        cw[1] = '\0';

        int retval = rig_send_morse(rig, RIG_VFO_CURR, cw);
        hl_usleep(100 * 1000);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_send_morse error: %s\n", __func__,
                      rigerror(retval));
        }
    }

    ELAPSED2;
    RETURNFUNC(RIG_OK);
}
