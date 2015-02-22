/*
 * intel.c
 *
 *  Created on: 2015. 2. 22.
 *      Author: SonienTaegi
 */
#include <stdio.h>
#include <assert.h>
#include <math.h>
#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

typedef
#ifdef __ARM_NEON__
		unsigned char
#else
		unsigned int
#endif
		MD_ACCUM;
//
unsigned int WIDTH;
unsigned int HEIGHT;
unsigned int FRAMERATE;

unsigned int md_avg_shift;
unsigned int md_window_size;
unsigned int md_frame_filled;
unsigned int md_frame_index;
unsigned char** md_frame_array;
MD_ACCUM* md_accum;

void intel_init(int width, int height, int frame_rate) {
	WIDTH 		= width;
	HEIGHT 		= height;
	FRAMERATE	= frame_rate;
}

void moving_detection_init(unsigned int window_size) {
	// NEON 연산을 이용하기 위해 frames 는 2의 승수만 가능하나, 일단은 8 로 고정
	assert(window_size == 8);
	assert(md_frame_array == NULL);

	frexp(window_size, &md_avg_shift);
	md_avg_shift--;

	md_window_size 	= window_size;
	md_frame_filled = 0;
	md_frame_index 	= 0;
	md_frame_array 	= malloc((size_t)(sizeof(unsigned char*) * window_size));
	memset(md_frame_array, 0x00, sizeof(md_frame_array));
	for(int i = 0; i < window_size; i++) {
		md_frame_array[i] = malloc(WIDTH * HEIGHT);
	}
	md_accum = malloc(sizeof(MD_ACCUM) * WIDTH * HEIGHT);

	printf("md_avg_shift = %d\n", md_avg_shift);
}

void moving_detection_close() {
	for(int i = 0; i < md_window_size; i++) {
		if(md_frame_array[i]) {
			free(md_frame_array[i]);
		}
	}

	free(md_frame_array);
}

unsigned int moving_detection_check(unsigned char* frame) {
	unsigned int diff_sum = 0;

	if(md_frame_filled == md_window_size) {
	// 직전 8 프레임의 이미지 합을 구한다.
 #ifdef __ARM_NEON__
		uint16x8_t sum[16];
		for(int i = 0; i < WIDTH * HEIGHT ; i += 8) {
			int sum_index = 0;
			for(int j = 0; j < md_window_size; j+=2) {
				uint8x8_t a = vld1_u8(md_frame_array[j+0]+i);
				uint8x8_t b = vld1_u8(md_frame_array[j+1]+i);
				sum[sum_index++] = vaddl_u8(a, b);
			}

			for(int j = 0; j < sum_index; j+=2) {
				sum[sum_index++] = vaddq_u16(sum[j], sum[j+1]);
			}
			uint8x8_t avg = vqshrn_n_u16(sum[--sum_index], md_avg_shift);
			vst1_u8(md_accum + i, avg);
		}
#else
		for(int x = 0; x < WIDTH * HEIGHT; x++) {
			unsigned int sum = 0;
			for(int i = 0; i < md_window_size; i++) {
				sum += *(md_frame_array[i]+x);
			}
			md_accum[x] = sum >> md_avg_shift;
		}
#endif

	// 오차의 절대값의 합을 구한다.
#ifdef __ARM_NEON__
		uint16x8t diff_sum_t;
		memset(&diff_sum_t, 0x00, sizeof(diff_sum_t));
		for(int x = 0; x < WIDTH * HEIGHT; x+=8 ) {
			uint8x8_t a = vld1_u8(md_accum+x);
			uint8x8_t b = vld1_u8(frame+x);
			diff_sum_t = vabal_u8(diff_sum_t, a, b);
		}

		for(int i = 0; i < 8; i++) {
			diff_sum += vgetq_lane_u16(diff_sum_t, i);
		}
#else
		for(int x = 0; x < WIDTH * HEIGHT; x++) {
			diff_sum += abs((int)(md_accum[x] - frame[x]));
		}
#endif
	}
	else {
		md_frame_filled++;
	}

	memcpy(md_frame_array[md_frame_index++], (size_t)frame, WIDTH * HEIGHT);
	if(md_frame_index == md_window_size) {
		md_frame_index = 0;
	}

	return diff_sum;
}

int main(int argc, char **argv) {
	unsigned char src[16 * 16] = { 0 };
	intel_init(16, 16, 30);
	moving_detection_init(8);

	for(int j = 0; j < 16; j++) {
		for(int k = 0; k < 8; k++) {
			if(k == 4) {
				src[4] = 10;
			}
			else {
				src[4] = 0;
			}
			printf("[%02d] ERR = %d\n", j, moving_detection_check(src));
		}
	}
	moving_detection_close();
}
