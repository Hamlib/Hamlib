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

#include <string.h>
#include "hamlib/rig.h"
#include "iofunc.h"
#include "register.h"
#include "riglist.h"
#include "guohetec.h"

// Common response validation function implementations
int validate_packet_header(const unsigned char *reply, const char *func_name)
{
    if (reply[0] != 0xA5 || reply[1] != 0xA5 || 
        reply[2] != 0xA5 || reply[3] != 0xA5) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid packet header, using cached values\n", func_name);
        return -1;
    }
    return 0;
}

int validate_data_length(const unsigned char *reply, int reply_size, const char *func_name)
{
    if (reply[4] == 0 || reply[4] > reply_size - 5) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid data length %d, using cached values\n", func_name, reply[4]);
        return -1;
    }
    return 0;
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

// Common response validation functions

/**
 * Read rig response with validation
 * @param rig RIG structure
 * @param reply Reply buffer
 * @param reply_size Size of reply buffer
 * @param func_name Function name for debug messages
 * @return 0 on success, -1 on error
 */
int read_rig_response(RIG *rig, unsigned char *reply, int reply_size, 
                     const char *func_name)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int ret;
    
    // Read header
    ret = read_block(rp, reply, 5);
    if (ret < 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to read header, using cached values\n", func_name);
        return -1;
    }
    
    // Validate data length
    if (reply[4] == 0 || reply[4] > reply_size - 5) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid data length %d, using cached values\n", func_name, reply[4]);
        return -1;
    }
    
    // Read data section
    ret = read_block(rp, &reply[5], reply[4]);
    if (ret < 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: Failed to read data, using cached values\n", func_name);
        return -1;
    }
    
    // Validate response length matches expected
    if (ret != reply[4]) {
        rig_debug(RIG_DEBUG_ERR, "%s: Data read mismatch: expected %d, got %d, using cached values\n", 
                 func_name, reply[4], ret);
        return -1;
    }
    
    return 0;
}

/**
 * Validate basic rig response
 * @param rig RIG structure
 * @param reply Reply buffer
 * @param reply_size Size of reply buffer
 * @param func_name Function name for debug messages
 * @return 0 on success, -1 on error
 */
int validate_rig_response(RIG *rig, const unsigned char *reply, int reply_size, 
                         const char *func_name)
{
    // Validate packet header
    if (validate_packet_header(reply, func_name) < 0) {
        return -1;
    }
    
    // Validate data length
    if (validate_data_length(reply, reply_size, func_name) < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * Validate frequency response with CRC check
 * @param rig RIG structure
 * @param reply Reply buffer
 * @param reply_size Size of reply buffer
 * @param func_name Function name for debug messages
 * @return 0 on success, -1 on error
 */
int validate_freq_response(RIG *rig, const unsigned char *reply, int reply_size, 
                          const char *func_name)
{
    // Basic validation
    if (validate_rig_response(rig, reply, reply_size, func_name) < 0) {
        return -1;
    }
    
    // Validate buffer boundaries for CRC
    int expected_total_length = 5 + reply[4] + 2; // header(5) + data_length + CRC(2)
    if (expected_total_length > reply_size) {
        rig_debug(RIG_DEBUG_ERR, "%s: Response too large for buffer: %d > %d, using cached values\n", 
                 func_name, expected_total_length, reply_size);
        return -1;
    }
    
    // CRC check
    uint16_t recv_crc = (reply[31] << 8) | reply[32]; // Last 2 bytes are CRC
    uint16_t calc_crc = CRC16Check(&reply[4], 27);
    if (recv_crc != calc_crc) {
        rig_debug(RIG_DEBUG_ERR, "%s: CRC check failed (received: %04X, calculated: %04X), using cached values\n", 
                 func_name, recv_crc, calc_crc);
        return -1;
    }
    
    // Validate frequency field offset
    int freq_b_offset = 13; // VFOB frequency starting position
    if (freq_b_offset + 3 >= expected_total_length - 2) { // -2 for CRC
        rig_debug(RIG_DEBUG_ERR, "%s: Frequency field offset out of bounds, using cached values\n", func_name);
        return -1;
    }
    
    return 0;
}

/**
 * Validate mode response with bounds checking
 * @param rig RIG structure
 * @param reply Reply buffer
 * @param reply_size Size of reply buffer
 * @param func_name Function name for debug messages
 * @param min_length Minimum required data length
 * @return 0 on success, -1 on error
 */
int validate_mode_response(RIG *rig, const unsigned char *reply, int reply_size, 
                          const char *func_name, int min_length)
{
    // Basic validation
    if (validate_rig_response(rig, reply, reply_size, func_name) < 0) {
        return -1;
    }
    
    // Validate minimum length for mode data
    if (reply[4] < min_length) {
        rig_debug(RIG_DEBUG_ERR, "%s: Response too short for mode data, using cached values\n", func_name);
        return -1;
    }
    
    // Validate mode field indices are within bounds
    if (reply[7] >= GUOHE_MODE_TABLE_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid mode A index %d, using cached values\n", func_name, reply[7]);
        return -1;
    }
    
    if (reply[8] >= GUOHE_MODE_TABLE_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: Invalid mode B index %d, using cached values\n", func_name, reply[8]);
        return -1;
    }
    
    return 0;
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
    uint8_t cmd[PMR171_CMD_LENGTH] = {
        0xA5, 0xA5, 0xA5, 0xA5, 
        0x03,                   
        0x0B,                   
        0x00, 0x00              
    };

    int orig_rate = port->parm.serial.rate;
    int orig_timeout = port->timeout;

    const int rates[] = {9600, 19200, 38400, 57600, 115200, 0};
    
    for (int i = 0; rates[i]; i++) {
        uint8_t reply[PMR171_REPLY_LENGTH];
        
        port->parm.serial.rate = rates[i];
        port->timeout = 500; 
        
        uint16_t crc = CRC16Check(&cmd[4], 2);
        cmd[6] = crc >> 8;
        cmd[7] = crc & 0xFF;
        
        rig_flush(port);
        
        int retval = write_block(port, cmd, PMR171_CMD_LENGTH);
        if (retval != RIG_OK) {
            continue;
        }
        
        retval = read_block(port, reply, 6);
        if (retval < 6 || memcmp(reply, "\xA5\xA5\xA5\xA5", 4) != 0) {
            continue;
        }
        
        uint8_t pkt_len = reply[4];
        if (pkt_len < 2 || pkt_len > (PMR171_REPLY_LENGTH - 5)) {
            continue;
        }
        
        retval = read_block(port, &reply[6], pkt_len - 1);
        if (retval < (pkt_len - 1)) {
            continue;
        }
        
        uint16_t recv_crc = (reply[pkt_len + 4] << 8) | reply[pkt_len + 5];
        uint16_t calc_crc = CRC16Check(&reply[4], pkt_len + 1);
        
        if (recv_crc != calc_crc) {
            continue;
        }
        
        if (reply[5] != 0x0B) {
            continue;
        }
        
        uint32_t freq = (reply[9] << 24) | (reply[10] << 16) | 
                       (reply[11] << 8) | reply[12];
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

