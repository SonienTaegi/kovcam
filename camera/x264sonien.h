/*
 * h264sonien.h
 *
 *  Created on: 2015. 1. 28.
 *      Author: SDS
 */

#ifndef SRC_X264SONIEN_H_
#define SRC_X264SONIEN_H_

#include <stddef.h>
#include <stdint.h>
#include <x264.h>

#define NUM_OF_THREAD	4

int x264_init(int width, int height, int fps);
int x264_get_sps_pps(x264_nal_t** nals, int* num_of_nals);
int x264_encode(unsigned char* yuv, x264_nal_t** pp_nal);
void x264_close();

#endif /* SRC_X264SONIEN_H_ */
