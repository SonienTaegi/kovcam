/*
 * intel.h
 *
 *  Created on: 2015. 2. 22.
 *      Author: SonienTaegi
 */

#ifndef CAMERA_INTEL_H_
#define CAMERA_INTEL_H_

/**
 * Performance comparing based o Odroid-C : Processing 100 frames with -O3
 *
 * C 	: 12.5s
 * NEON	:  3.4s
 */

void intel_init(int width, int height, int frame_rate);

void moving_detection_init(unsigned int window_size);

void moving_detection_close();

unsigned int moving_detection_check(unsigned char* frame);

#endif /* CAMERA_INTEL_H_ */
