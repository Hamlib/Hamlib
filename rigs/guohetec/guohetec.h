// rigs/guohetec/guohetec.h
#ifndef _guohetec_H_
#define _guohetec_H_

#include <hamlib/rig.h>

#define PMR171_CMD_LENGTH 8
#define PMR171_REPLY_LENGTH 24

#define GUOHE_MODE_TABLE_MAX 8  // 协议定义的部分模式不支持,只有8种

extern struct rig_caps pmr171_caps;
extern struct rig_caps q900_caps;

uint16_t CRC16Check(const unsigned char *buf, int len);
rmode_t guohe2rmode(unsigned char mode, const rmode_t mode_table[]);
unsigned char rmode2guohe(rmode_t mode, const rmode_t mode_table[]);
unsigned char *to_be(unsigned char data[], unsigned long long freq, unsigned int byte_len);
unsigned long long from_be(const unsigned char data[],unsigned int byte_len);


#endif // _guohetec_H_
