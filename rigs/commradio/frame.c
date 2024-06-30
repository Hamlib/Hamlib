/*
 * Hamlib CommRadio backend
 * idk, copyright and GPL here
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hamlib/rig.h>
#include <serial.h>

#include "frame.h"


/*
 * Brute-force determined to be polynomial = 0x1021.
 * Seems to be the same CRC16 as Kermit. Initialized with 0.
 */
uint16_t crc16tab[] = {
	0,      4489,  8978, 12955, 17956, 22445, 25910, 29887, 35912, 40385,
    44890, 48851, 51820, 56293, 59774, 63735,  4225,   264, 13203,  8730,
    22181, 18220, 30135, 25662, 40137, 36160, 49115, 44626, 56045, 52068,
    63999, 59510,  8450, 12427,   528,  5017, 26406, 30383, 17460, 21949,
    44362, 48323, 36440, 40913, 60270, 64231, 51324, 55797, 12675,  8202,
     4753,   792, 30631, 26158, 21685, 17724, 48587, 44098, 40665, 36688,
    64495, 60006, 55549, 51572, 16900, 21389, 24854, 28831,  1056,  5545,
    10034, 14011, 52812, 57285, 60766, 64727, 34920, 39393, 43898, 47859,
    21125, 17164, 29079, 24606,  5281,  1320, 14259,  9786, 57037, 53060,
    64991, 60502, 39145, 35168, 48123, 43634, 25350, 29327, 16404, 20893,
     9506, 13483,  1584,  6073, 61262, 65223, 52316, 56789, 43370, 47331,
    35448, 39921, 29575, 25102, 20629, 16668, 13731,  9258,  5809,  1848,
    65487, 60998, 56541, 52564, 47595, 43106, 39673, 35696, 33800, 38273,
    42778, 46739, 49708, 54181, 57662, 61623,  2112,  6601, 11090, 15067,
    20068, 24557, 28022, 31999, 38025, 34048, 47003, 42514, 53933, 49956,
    61887, 57398,  6337,  2376, 15315, 10842, 24293, 20332, 32247, 27774,
    42250, 46211, 34328, 38801, 58158, 62119, 49212, 53685, 10562, 14539,
     2640,  7129, 28518, 32495, 19572, 24061, 46475, 41986, 38553, 34576,
    62383, 57894, 53437, 49460, 14787, 10314,  6865,  2904, 32743, 28270,
    23797, 19836, 50700, 55173, 58654, 62615, 32808, 37281, 41786, 45747,
    19012, 23501, 26966, 30943,  3168,  7657, 12146, 16123, 54925, 50948,
    62879, 58390, 37033, 33056, 46011, 41522, 23237, 19276, 31191, 26718,
     7393,  3432, 16371, 11898, 59150, 63111, 50204, 54677, 41258, 45219,
    33336, 37809, 27462, 31439, 18516, 23005, 11618, 15595,  3696,  8185,
    63375, 58886, 54429, 50452, 45483, 40994, 37561, 33584, 31687, 27214,
    22741, 18780, 15843, 11370,  7921,  3960                                                           
    };

static uint16_t crc16byte(const unsigned char byte, uint16_t crc)
{
	return crc16tab[(crc & 0xFF) ^ byte] ^ (crc >> 8);
}

static uint16_t crc16(const unsigned char *data, int data_len, uint16_t crc)
{
	for (int i = 0; i < data_len; i++)
	{
		crc = crc16byte(data[i], crc);
	}
	return crc;
}

static int esc_append(unsigned char frame[], int idx, const unsigned char a)
{
	switch(a)
	{
		case CR_SOF:
		case CR_EOF:
		case CR_ESC:
			frame[idx] = CR_ESC;
			frame[idx+1] = a ^ 20;
			return idx+2;
			break;
		default:
			frame[idx] = a;
			return idx+1;
	}
}

/*
 * frame[] might be 2x the size of data and crc +3 bytes if it is just a long
 * string of 0xFC, 0xFD, or 0xFE for some insane reason.
 *
 */
int frame_message(unsigned char frame[], const unsigned char *data, 
		          const int data_len)
{
	uint16_t crc = 0;
	frame[0] = CR_SOF;
	frame[1] = 0x21; /* Messages to the radio are always 0x21 */
	crc = crc16byte(frame[1], crc);
	frame[2] = data[0];
	crc = crc16byte(frame[2], crc);
	int frame_len = 3;
	for (int i = 1; i < data_len; i++) 
	{
		crc = crc16byte(data[i], crc);
		frame_len = esc_append(frame, frame_len, data[i]);
	}
	frame_len = esc_append(frame, frame_len, crc >> 8);
	frame_len = esc_append(frame, frame_len, crc & 0xFF);
	frame[frame_len] = CR_EOF;
	frame_len++;
	return frame_len;
}

int unpack_frame(unsigned char msg[], const unsigned char *frame, 
		         const int frame_len)
{
	if (frame_len < 5) 
	{
		rig_debug(RIG_DEBUG_ERR, 
				"%s Got a frame that was too small (<5) to be valid\n", 
				__func__);
		return -RIG_ETRUNC;
	}
    if ((frame[0] != CR_SOF) || (frame[frame_len-1] != CR_EOF))
	{
		rig_debug(RIG_DEBUG_ERR, 
				"%s Tried to unpack a frame without start or end\n", __func__);
		return -RIG_EPROTO; 
	}
	if (frame[1] != 0x11)
	{
		rig_debug(RIG_DEBUG_ERR,
				"%s Message address is not for host (0x11)\n", __func__);
		return -RIG_EPROTO;
	}
	int msg_len = 0;
	for (int i = 2; i < frame_len; i++)
	{
		switch (frame[i])
		{
			case CR_SOF:
				return -RIG_EPROTO;
				break;
			case CR_EOF:
				i = frame_len;
				break;
			case CR_ESC:
				i++;
				msg[msg_len] = frame[i] ^ 20;
				msg_len++;
				break;
			default:
				msg[msg_len] = frame[i];
				msg_len++;
		}
	}
	uint16_t msg_crc = (msg[msg_len-2] << 8) | msg[msg_len-1];
	msg_len = msg_len-2;
	uint16_t crc = crc16(msg, msg_len, crc16byte(frame[1],0));
	if (msg_crc != crc) 
	{
		rig_debug(RIG_DEBUG_ERR, "%s CRC check failed. msg_crc=%x, crc=%x\n", __func__, msg_crc, crc);
	}
	return msg_len;
}


