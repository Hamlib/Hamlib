/*
 *  Hamlib GUOHETEC backend - common functions
 *  Copyright (c) 2024 by GUOHETEC
 *
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

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "iofunc.h"
#include "register.h"
#include "riglist.h"
#include "guohetec.h"

enum
{
    GUOHE_FRAME_HEADER_LENGTH = 5,
    GUOHE_STATUS_MIN_PACKET_LENGTH = 15,
    GUOHE_STATUS_COMMAND = 0x0B
};

static int validate_packet_header(const unsigned char *reply,
                                  const char *func_name)
{
    if (reply[0] != 0xA5 || reply[1] != 0xA5 || 
        reply[2] != 0xA5 || reply[3] != 0xA5) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid packet header, using cached values\n", func_name);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

// CRC16/CCITT-FALSE
uint16_t CRC16Check(const unsigned char *buf, int len)
{
    uint16_t crc = 0xFFFF; // Initial value
    uint16_t polynomial = 0x1021; // Polynomial x^16 + x^12 + x^5 + 1

    for (int i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)buf[i] << 8); // XOR byte into the upper 8 bits of crc

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ polynomial;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc;
}


 rmode_t guohe2rmode(unsigned char mode, const rmode_t mode_table[])
 {
     rig_debug(RIG_DEBUG_VERBOSE, "%s called, mode=0x%02x\n", __func__,
               mode);
 
     if (mode >= GUOHE_MODE_TABLE_MAX)
     {
         return (RIG_MODE_NONE);
     }
 
    rig_debug(RIG_DEBUG_VERBOSE, "%s: returning %s\n", __func__,
               rig_strrmode(mode_table[mode]));

     return (mode_table[mode]);
 }
 
 unsigned char rmode2guohe(rmode_t mode, const rmode_t mode_table[])
 {
     rig_debug(RIG_DEBUG_VERBOSE, "%s called, mode=%s\n", __func__,
               rig_strrmode(mode));
 
     if (mode != RIG_MODE_NONE)
     {
         unsigned char i;
 
         for (i = 0; i < GUOHE_MODE_TABLE_MAX; i++)
         {
             if (mode_table[i] == mode)
             {
                 rig_debug(RIG_DEBUG_VERBOSE, "%s: returning 0x%02x\n", __func__, i);
                 return (i);
             }
         }
     }
 
     return (-1);
 }
 
 /**
  * Convert to big-endian byte order
 * @param data Data pointer
 * @param freq Frequency value
 * @param byte_len Byte length
  */
 unsigned char *to_be(unsigned char data[],
                      unsigned long long freq,
                      unsigned int byte_len)
 {
     int i;
 
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

     for (i = byte_len - 1; i >= 0; i--)
     {
         unsigned char a = freq & 0xFF;
         freq >>= 8;
         data[i] = a;
     }
 
     return data;
 }
 
 /**
  * Convert from big-endian byte order
 * @param data Data pointer
 * @param byte_len Byte length
 * @return Converted value
  */
 unsigned long long from_be(const unsigned char data[], unsigned int byte_len)
 {
     unsigned long long result = 0;
     int i;
 
     rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

     for (i = 0; i < byte_len; i++)
     {
         result = (result << 8) | data[i];
     }
 
     return result;
 }

int guohetec_read_response(RIG *rig, unsigned char *reply, size_t reply_size,
                           const char *func_name)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int ret;

    if (reply_size < GUOHE_FRAME_HEADER_LENGTH)
    {
        return -RIG_EINVAL;
    }

    ret = read_block(rp, reply, GUOHE_FRAME_HEADER_LENGTH);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to read header, using cached values\n", func_name);
        return ret;
    }

    if (validate_packet_header(reply, func_name) < 0)
    {
        return -RIG_EPROTO;
    }

    if (reply[4] == 0 || reply[4] > reply_size - GUOHE_FRAME_HEADER_LENGTH)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid data length %d, using cached values\n", func_name, reply[4]);
        return -RIG_EPROTO;
    }

    ret = read_block(rp, &reply[GUOHE_FRAME_HEADER_LENGTH], reply[4]);

    if (ret < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to read data, using cached values\n", func_name);
        return ret;
    }

    if (ret != reply[4])
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Data read mismatch: expected %d, got %d, using cached values\n",
                  func_name, reply[4], ret);
        return -RIG_EPROTO;
    }

    return GUOHE_FRAME_HEADER_LENGTH + ret;
}

int guohetec_decode_status(const unsigned char *reply, size_t reply_size,
                           struct guohetec_status *status,
                           const char *func_name)
{
    size_t expected_size;
    size_t crc_offset;
    uint16_t received_crc;
    uint16_t calculated_crc;

    if (reply == NULL || status == NULL || func_name == NULL)
    {
        return -RIG_EINVAL;
    }

    if (reply_size < GUOHE_FRAME_HEADER_LENGTH ||
            validate_packet_header(reply, func_name) < 0)
    {
        return -RIG_EPROTO;
    }

    if (reply[4] < GUOHE_STATUS_MIN_PACKET_LENGTH)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Status response too short: %u, using cached values\n",
                  func_name, reply[4]);
        return -RIG_EPROTO;
    }

    expected_size = GUOHE_FRAME_HEADER_LENGTH + reply[4];

    if (reply_size != expected_size)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Status response length mismatch: expected %zu, got %zu, using cached values\n",
                  func_name, expected_size, reply_size);
        return -RIG_EPROTO;
    }

    if (reply[5] != GUOHE_STATUS_COMMAND)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: Unexpected status response command: 0x%02X, using cached values\n",
                  func_name, reply[5]);
        return -RIG_EPROTO;
    }

    crc_offset = expected_size - 2;
    received_crc = ((uint16_t)reply[crc_offset] << 8) |
                   reply[crc_offset + 1];
    calculated_crc = CRC16Check(&reply[4], reply[4] - 1);

    if (received_crc != calculated_crc)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: CRC check failed (received: %04X, calculated: %04X), using cached values\n",
                  func_name, received_crc, calculated_crc);
        return -RIG_EPROTO;
    }

    status->ptt = reply[6];
    status->mode_a = reply[7];
    status->mode_b = reply[8];
    status->freq_a = (uint32_t)from_be(&reply[9], 4);
    status->freq_b = (uint32_t)from_be(&reply[13], 4);
    status->vfo = reply[17] == 1 ? RIG_VFO_B : RIG_VFO_A;

    return RIG_OK;
}

int guohetec_get_status(RIG *rig, struct guohetec_status *status,
                        const char *func_name)
{
    unsigned char command[GUOHE_STATUS_CMD_LENGTH] = {
        0xA5, 0xA5, 0xA5, 0xA5, 0x03, GUOHE_STATUS_COMMAND, 0x00, 0x00
    };
    unsigned char reply[GUOHE_MAX_FRAME_LENGTH];
    hamlib_port_t *rp = RIGPORT(rig);
    uint16_t crc;
    int ret;

    crc = CRC16Check(&command[4], 2);
    command[6] = crc >> 8;
    command[7] = crc & 0xFF;

    rig_flush(rp);
    ret = write_block(rp, command, sizeof(command));

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to write status request\n", func_name);
        return ret;
    }

    ret = guohetec_read_response(rig, reply, sizeof(reply), func_name);

    if (ret < 0)
    {
        return ret;
    }

    return guohetec_decode_status(reply, (size_t)ret, status, func_name);
}

// Initialization function
DECLARE_INITRIG_BACKEND(guohetec) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Initializing guohetec \n", __func__);

    // Register driver to hamlib
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Loading guohetec backend\n", __func__);
    rig_register(&pmr171_caps);
    rig_register(&q900_caps);

    return RIG_OK;
}

// Probe function implementation
DECLARE_PROBERIG_BACKEND(guohetec) {
    uint8_t cmd[GUOHE_STATUS_CMD_LENGTH] = {
        0xA5, 0xA5, 0xA5, 0xA5, 
        0x03,                   
        0x0B,                   
        0x00, 0x00              
    };

    int orig_rate = port->parm.serial.rate;
    int orig_timeout = port->timeout;

    const int rates[] = {9600, 19200, 38400, 57600, 115200, 0};
    
    for (int i = 0; rates[i]; i++) {
        uint8_t reply[GUOHE_MAX_FRAME_LENGTH];
        struct guohetec_status status;
        
        port->parm.serial.rate = rates[i];
        port->timeout = 500; 
        
        uint16_t crc = CRC16Check(&cmd[4], 2);
        cmd[6] = crc >> 8;
        cmd[7] = crc & 0xFF;
        
        rig_flush(port);
        
        int retval = write_block(port, cmd, GUOHE_STATUS_CMD_LENGTH);
        if (retval != RIG_OK) {
            continue;
        }
        
        retval = read_block(port, reply, GUOHE_FRAME_HEADER_LENGTH);
        if (retval < GUOHE_FRAME_HEADER_LENGTH ||
                validate_packet_header(reply, __func__) < 0) {
            continue;
        }
        
        uint8_t pkt_len = reply[4];
        if (pkt_len == 0 ||
                pkt_len > (GUOHE_MAX_FRAME_LENGTH - GUOHE_FRAME_HEADER_LENGTH)) {
            continue;
        }
        
        retval = read_block(port, &reply[GUOHE_FRAME_HEADER_LENGTH], pkt_len);
        if (retval != pkt_len) {
            continue;
        }

        if (guohetec_decode_status(reply,
                                   GUOHE_FRAME_HEADER_LENGTH + pkt_len,
                                   &status, __func__) < 0) {
            continue;
        }

        uint32_t freq = status.freq_a;
        if (freq < 100000 || freq > 470000000) {
            continue;
        }
        
        port->parm.serial.rate = orig_rate;
        port->timeout = orig_timeout;
        
        if (cfunc) {
            (*cfunc)(port, RIG_MODEL_PMR171, data);
        }
        
        return RIG_MODEL_PMR171;
    }
    
    port->parm.serial.rate = orig_rate;
    port->timeout = orig_timeout;
    
    return RIG_MODEL_NONE;
}
