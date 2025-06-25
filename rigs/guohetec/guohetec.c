#include <unistd.h>
#include <string.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "register.h"
#include "guohetec.h"
#include "misc.h"


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
 
 
 unsigned long long from_be(const unsigned char data[],
                            unsigned int byte_len)
 {
     int i;
     unsigned long long f = 0;
 
     rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
 
     for (i = 0; i < byte_len; i++)
     {
         f = (f << 8) + data[i];
     }
 
     return f;
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
    
    uint8_t reply[PMR171_REPLY_LENGTH];
    int retval;

    int orig_rate = port->parm.serial.rate;
    int orig_timeout = port->timeout;

    const int rates[] = {9600, 19200, 38400, 57600, 115200, 0};
    
    for (int i = 0; rates[i]; i++) {
        port->parm.serial.rate = rates[i];
        port->timeout = 500; 
        
        uint16_t crc = CRC16Check(&cmd[4], 2);
        cmd[6] = crc >> 8;
        cmd[7] = crc & 0xFF;
        
        rig_flush(port);
        
        retval = write_block(port, cmd, PMR171_CMD_LENGTH);
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

