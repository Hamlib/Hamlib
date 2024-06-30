/*
 * Hamlib CommRadio backend
 * idk, copyright and GPL here
 */

#ifndef _FRAME_H
#define _FRAME_H

#define CR_SOF 0xFE
#define CR_EOF 0xFD 
#define CR_ESC 0xFC

//TODO: Find out what the frame length actually is for IQ/spectrum data
#define CR_FRAMELENGTH 256

int frame_message(unsigned char frame[], const unsigned char *data, 
		          int data_len);

int unpack_frame(unsigned char msg[], const unsigned char *frame, int frame_len);

#endif /* _FRAME_H */

