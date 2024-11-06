/*
 * Hamlib CommRadio backend
 * idk, copyright and GPL here
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include <register.h>
#include <serial.h>

#include "misc.h"

#include "commradio.h"
#include "frame.h"


/*
 * As far as I can tell, the commands and their structure are the same for the
 * CR-1a as they are for the CTX-10, but I don't have a CR1a to test with.
 * I'm putting these functions here in case they are reusable.
 */

int commradio_transaction(RIG *rig, const unsigned char *cmd, int cmd_len,
                          unsigned char *data, int *data_len)
{
    int ret = -RIG_EINTERNAL;
    struct rig_state *rs;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;

    rs = STATE(rig);
    rs->transaction_active = 1;

    /*
     * Flush is needed until async mode is done. The CTX-10 sends frames every
     * time the VFO changes.
     */
    rig_flush(rp);

    int frame_len;
    unsigned char frame[3 + 2 * (cmd_len + 2)];
    size_t rx_len = CR_FRAMELENGTH;
    unsigned char rx[rx_len];
    frame_len = frame_message(frame, cmd, cmd_len);
    ret = write_block(rp, frame, frame_len);

    if (ret < RIG_OK)
    {
        goto transaction_quit;
    }

    const char stopset[] = { CR_EOF };
    ret = read_string(rp, rx, rx_len - 1, stopset, 1, 0, 1);

    if (ret < RIG_OK)
    {
        goto transaction_quit;
    }

    ret = commradio_unpack_frame(data, rx, ret);

    if (ret < RIG_OK)
    {
        goto transaction_quit;
    }

    *data_len = ret;
    //TODO: check for error response 0x11

transaction_quit:
    rs->transaction_active = 0;
    RETURNFUNC(ret);
}


int commradio_init(RIG *rig)
{
    ENTERFUNC;
    // I can't think of anything that goes in here yet.
    RETURNFUNC(RIG_OK);
}

int commradio_cleanup(RIG *rig)
{
    ENTERFUNC;
    // dealloc stuff if it gets added to _init
    RETURNFUNC(RIG_OK);
}

int commradio_rig_open(RIG *rig)
{
    ENTERFUNC;
    // Possibly check if our serial port is configured right and we are not
    // doing bad things to the GPIO lines
    RETURNFUNC(RIG_OK);
}

int commradio_rig_close(RIG *rig)
{
    ENTERFUNC;
    // i don't really know
    RETURNFUNC(RIG_OK);
}

/*
 * The CTX-10 sends VFO frequency updates when the knob is turned by the
 * operator, so this is really a good case for async events. Unfortunately
 * I can't find any good examples of how to do that, so instead just flush
 * and send a request...
 */

int commradio_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    const unsigned char cmd[] = {0x32}; // Get frequency request
    unsigned char data[CR_FRAMELENGTH];
    int data_len;
    int ret = -RIG_EINTERNAL;

    ENTERFUNC;

    ret = commradio_transaction(rig, cmd, 1, data, &data_len);

    if (data_len == 5 && (data[0] == 0x33 || data[0] == 0x34))
    {
        *freq = (data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4]);
        ret = RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected response to 0x32\n", __func__);
    }

    RETURNFUNC(ret);
}


int commradio_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char data[CR_FRAMELENGTH];
    int data_len;
    int ret = -RIG_EINTERNAL;

    ENTERFUNC;

    if (freq < 150000 || freq > 30000000)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    uint32_t int_freq = freq;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Got freq=%f, int_freq=%u\n", __func__,
              freq, int_freq);
    unsigned char cmd[] =
    {
        0x30, // Set frequency request
        0xFF & (int_freq >> 24),
        0xFF & (int_freq >> 16),
        0xFF & (int_freq >> 8),
        0xFF & (int_freq)
    };

    ret = commradio_transaction(rig, cmd, 5, data, &data_len);

    if (data_len == 5 && (data[0] == 0x31 || data[0] == 0x34))
    {
        uint32_t new_freq = (data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4]);

        if (int_freq == new_freq)
        {
            RETURNFUNC(RIG_OK);
        }
        else
        {
            RETURNFUNC(-RIG_ERJCTED);
        }
    }
    // CTX-10 returns 11 02 30 00 00 00 01 if we try to go out of its
    // general-coverage frequency range 150kHz - 30MHz. I'm not sure why Hamlib
    // even tries to do this, since its defined in the caps...
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected response to 0x30\n", __func__);
        ret = -RIG_ERJCTED;
    }

    RETURNFUNC(ret);
}

/*
 * Stubs. I'm not aware of a way to get or set the mode on the CTX-10.
 */
int commradio_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    return (RIG_OK);
}

int commradio_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    *mode = RIG_MODE_NONE;
    return (RIG_OK);
}

/*
 * Stubs. The CTX-10 has only one VFO and split mode doesn't change how it
 * responds via the serial port.
 */
int commradio_set_vfo(RIG *rig, vfo_t vfo)
{
    return (RIG_OK);
}

int commradio_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = RIG_VFO_A;
    return (RIG_OK);
}


DECLARE_INITRIG_BACKEND(commradio)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);
    rig_register(&ctx10_caps);

    return (RIG_OK);
}

/*
 * For some reason, I can't get this to even link without this function.
 */
DECLARE_PROBERIG_BACKEND(commradio)
{
    return (RIG_MODEL_NONE);
}

