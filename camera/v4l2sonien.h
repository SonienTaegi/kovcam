/*
 * v4l2sonien.h
 *
 *  Created on: 2015. 1. 19.
 *      Author: SonienTaegi
 */

#ifndef V4L2SONIEN_H_
#define V4L2SONIEN_H_

typedef struct {
	void* ptr;
	int length;
} CAPTURE_BUFFER;

/**
 * x264 지원을 위해 YUV422 -> I420 변환 로직 추가
 */
void convert_yuv422_to_i420(unsigned char* yuyv, unsigned char* i420, int width, int height);

/*
 * 디바이스 파일 열기
 */
int open_device(char* szDeviceID);

/*
 * 제원 확인
 */
void print_capabailty(int fd);

/*
 * Device Type 확인
 */
int get_video_type(int fd);

/*
 * Capturing format 을 설정한다.
 * type : NULL 시 Video Format 으로 장비 기본값을 사용한다.
 */
void set_video_format(int fd, int width, int height, unsigned int type);

/*
 * Frame rate 설정.
 */
float set_video_fps(int fd, int fps);

/*
 * Memory Mapped Stream Buffer 를 할당한다.
 */
void allocate_video_mmap_buffer(int fd, int* num_of_buffers, CAPTURE_BUFFER** pBuffers);

/*
 * 최초로 모든 버퍼메모리를 큐한다.
 */
void prepare_video_mmap_buffer(int fd, int num_of_buffers);

/*
 * Video stream buffer 를 해제한다.
 */
void release_video_mmap_buffer(int fd, CAPTURE_BUFFER* buffers, int num_of_buffers);

/*
 * Video input stream 을 시작한다.
 */
void video_stream_on(int fd);

/*
 * Video input stream 을 종료한다.
 */
void video_stream_off(int fd);

#endif /* V4L2SONIEN_H_ */
