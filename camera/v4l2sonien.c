#include "v4l2sonien.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

void convert_yuv422_to_i420(unsigned char* yuyv, unsigned char* i420, int width, int height) {
	unsigned char* pSrc  = yuyv;
	unsigned char* pDstY = i420;
	unsigned char* pDstU = pDstY + width * height;
	unsigned char* pDstV = pDstY + width * height * 5 / 4;
	bool isEvenRow = true;

#ifdef __ARM_NEON__
#pragma message("Uses NEON to convert yuv422 to i420")
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x += 16) {
			int i = y * width + x;
			uint8_t uv[16];
			uint8x8x2_t src;

			src = vld2_u8(pSrc); pSrc+=16;
			vst1_u8(pDstY,	src.val[0]); pDstY+=8;
			if(isEvenRow)
				vst1_u8(uv,   src.val[1]);

			src = vld2_u8(pSrc); pSrc+=16;
			vst1_u8(pDstY, 	src.val[0]); pDstY+=8;
			if(isEvenRow)
				vst1_u8(uv+8, src.val[1]);

			if(isEvenRow) {
				src = vld2_u8(uv);
				vst1_u8(pDstU, 	src.val[0]);	pDstU+=8;
				vst1_u8(pDstV, 	src.val[1]);	pDstV+=8;
			}
		}
		isEvenRow = !isEvenRow;
	}
#else
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x+=2) {
			*(pDstY++) = *(pSrc++);
			if(isEvenRow) {
				*(pDstU++) = *(pSrc++);
			}
			else {
				pSrc++;
			}
			*(pDstY++) = *(pSrc++);
			if(isEvenRow) {
				*(pDstV++) = *(pSrc++);
			}
			else {
				pSrc++;
			}
		}
		isEvenRow = !isEvenRow;
	}
#endif
}


/*
 * 디바이스 파일 열기
 */
int open_device(char* szDeviceID) {
	int fd = open(szDeviceID, O_RDWR);
	if(fd < 0) {
		printf("[FAIL] Can not open device %s.\n", szDeviceID);
		exit(EXIT_FAILURE);
	}
	printf("[ OK ] Open device %s.\n", szDeviceID);

	return fd;
}

/*
 * 제원 확인
 */
void print_capabailty(int fd) {
	struct v4l2_capability capability;
	if ( -1 == ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
		printf("[FAIL] Can not get device capability.\n");
		exit(EXIT_FAILURE);
	}
	printf("[INFO] Device capability\n");
	printf("       DRIVER NAME    : %s\n", capability.driver);
	printf("       CARD NAME      : %s\n", capability.card);
	printf("       BUS            : %s\n", capability.bus_info);
	printf("       VERSION        : %d\n", capability.version);
	printf("       * Supports ...\n");
	printf("         VIDEO IN     : %s\n", capability.capabilities & V4L2_CAP_VIDEO_CAPTURE ? "YES" : "NO");
	printf("         AUDIO IN     : %s\n", capability.capabilities & V4L2_CAP_AUDIO         ? "YES" : "NO");
	printf("         READ / WRITE : %s\n", capability.capabilities & V4L2_CAP_READWRITE     ? "YES" : "NO");
	printf("         STREAMING    : %s\n", capability.capabilities & V4L2_CAP_STREAMING     ? "YES" : "NO");
}

/*
 * Device video type 확인
 */
int get_video_type(int fd) {
	// 포트 번호 추출
	struct v4l2_input input;
	memset(&input, 0x00, sizeof(input));

	if ( -1 == ioctl(fd, VIDIOC_G_INPUT, &input.index)) {
		printf("[FAIL] Can not get port index.\n");
		exit(EXIT_FAILURE);
	}

	if( -1 == ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
		printf("[FAIL] Can not get input device type.\n");
		exit(EXIT_FAILURE);
	}
	printf("[INFO] Device type of (%d)%s is ", input.index, input.name);

	// 장비 유형 확인
	switch(input.type) {
	case V4L2_INPUT_TYPE_TUNER :
		printf("tunner");
		break;
	case V4L2_INPUT_TYPE_CAMERA :
		printf("camera");
		break;
	default :
		printf("is... what the fuck!?");
		break;
	}
	printf("\n");
	printf("[INFO] Covers 0x%08x\n", input.std);

	return input.type;
}

/*
 * Capturing format 을 설정한다.
 * type : -1 시 Video Format 으로 장비 기본값을 사용한다.
 */
void set_video_format(int fd, int width, int height, unsigned int type) {
	struct v4l2_format	format;
	memset(&format, 0x00, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	// 기본 설정 획득
	if( -1 == ioctl(fd, VIDIOC_G_FMT, &format)) {
		printf("[FAIL] Can not get device default format.\n");
		exit(EXIT_FAILURE);
	}
	struct v4l2_pix_format* pPix = &format.fmt.pix;
	pPix->width  = width;
	pPix->height = height;
	if(type != NULL) {
		pPix->pixelformat = type;
	}
	else {
		type = pPix->pixelformat;
	}
	pPix->field = V4L2_FIELD_NONE;
	if( -1 == ioctl(fd, VIDIOC_S_FMT, &format)) {
		printf("[FAIL] Can not set device format.\n");
		exit(EXIT_FAILURE);
	}

	pPix = &format.fmt.pix;
	printf("[INFO] W=%d H=%d F=", pPix->width, pPix->height);

	unsigned int fourcc 		= pPix->pixelformat;
	for(int i = 0; i < 4; i++) {
		printf("%c", fourcc & 0x000000ff);
		fourcc = fourcc >> 8;
	}
	printf("\n");

	if(pPix->width != width || pPix->height != height || pPix->pixelformat != type) {
		printf("[FAIL] Device doesn't support requested format.\n");
		exit(EXIT_FAILURE);
	}
	printf("[ OK ] Format set up.\n");
}

void allocate_video_mmap_buffer(int fd, int* num_of_buffers, CAPTURE_BUFFER** pBuffers) {
	struct v4l2_requestbuffers request;
	memset(&request, 0x00, sizeof(request));
	request.count 	= (*num_of_buffers);
	request.type  	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	request.memory 	= V4L2_MEMORY_MMAP;
	if ( -1 == ioctl(fd, VIDIOC_REQBUFS, &request)) {
		printf("[FAIL] Can not request memory mapped stream buffer.\n");
		exit(EXIT_FAILURE);
	}

	if(request.count < *num_of_buffers) {
		*num_of_buffers = request.count;
	}

	CAPTURE_BUFFER* buffers = malloc(sizeof(CAPTURE_BUFFER) * request.count);
	*pBuffers = buffers;

	for(int i = 0; i < request.count; i++) {
		struct v4l2_buffer buffer_query;
		memset(&buffer_query, 0x00, sizeof(buffer_query));
		buffer_query.index 	= i;
		buffer_query.type  	= request.type;
		buffer_query.memory	= V4L2_MEMORY_MMAP;

		ioctl(fd, VIDIOC_QUERYBUF, &buffer_query);
		buffers[i].length = buffer_query.length;
		buffers[i].ptr = mmap(NULL, buffer_query.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer_query.m.offset);
		if(buffers[i].ptr == MAP_FAILED) {
			printf("[FAIL] Can not allocate memory mapped buffer.\n");
			exit(EXIT_FAILURE);
		}
		printf("[ OK ] Allocates BUFFER[%2d] : %d bytes\n", i, buffer_query.length);
	}
}

float set_video_fps(int fd, int fps) {
	struct v4l2_streamparm param;
	memset(&param, 0x00, sizeof(param));

	param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	struct v4l2_captureparm* capture = &param.parm;
	capture->capability 	= V4L2_CAP_TIMEPERFRAME;
	capture->capturemode 	= V4L2_MODE_HIGHQUALITY;
	capture->timeperframe.numerator 	= 1;
	capture->timeperframe.denominator 	= fps;

	if ( -1 == ioctl(fd, VIDIOC_S_PARM, &param)) {
		printf("[FAIL] Can not set frame rate.\n");
		exit(EXIT_FAILURE);
	}

	float f_fps = capture->timeperframe.denominator / capture->timeperframe.numerator;
	printf("[ OK ] Determined frame rate is %.2f\n", f_fps);

	return f_fps;
}

void prepare_video_mmap_buffer(int fd, int num_of_buffers) {
	for(int i = 0; i < num_of_buffers; i++) {
		struct v4l2_buffer buffer;
		memset(&buffer, 0x00, sizeof(buffer));
		buffer.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory 	= V4L2_MEMORY_MMAP;
		buffer.index 	= i;

		if( -1 == ioctl(fd, VIDIOC_QBUF, &buffer)) {
			printf("[FAIL] : Can not prepare buffer.\n");
			exit(EXIT_FAILURE);
		}
	}

	printf("[ OK ] All buffers are queued.\n");
}

void video_stream_on(int fd) {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(fd, VIDIOC_STREAMON, &type)) {
		printf("[FAIL] : Can not turn stream ON.\n");
		exit(EXIT_FAILURE);
	}
	printf("[ OK ] Stream ON.\n");
}

void video_stream_off(int fd) {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(fd, VIDIOC_STREAMOFF, &type)) {
		printf("[FAIL] : Can not turn stream OFF.\n");
		exit(EXIT_FAILURE);
	}
	printf("[ OK ] Stream OFF.\n");
}

void release_video_mmap_buffer(int fd, CAPTURE_BUFFER* buffers, int num_of_buffers) {
	for(int i = 0; i < num_of_buffers; i++) {
		munmap(buffers[i].ptr, buffers[i].length);
	}
	free(buffers);

	printf("[ OK ] All buffers are released.\n");
}
