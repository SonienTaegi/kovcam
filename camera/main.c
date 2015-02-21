/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include <getopt.h>             /* getopt_long() */
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <semaphore.h>

#include "v4l2sonien.h"
#include "x264sonien.h"
#include "CircularQueue.h"
#include "CircularBuffer.h"

#define PID_FILE	"/tmp/kovcam.camera.pid"
#define clear(arg) memset(&arg, 0x00, sizeof(arg))

typedef struct CONTEXT {
	// 프로세스 정보
	bool 	isValidProcess;
	bool	isTerminated;
	bool	isDaemon;

	// 설정
	int		width;
	int 	height;
	int 	fps;
	int		frames_captured;
	bool	isYUV420;
	bool	isREADWRITE;
	char	device_name[256];
	char	output_name[1024];

	// 입출력 정보
	int		fd_cam;
	int		buffer_pool_size;
	CAPTURE_BUFFER* buffer_pool;

	// Semaphore 정보
	sem_t   sem_encoder;

	// Queue 정보
	CQ_INSTANCE* pCQ_encoder_In;

	// Stream Buffer 정보
	CB_INSTANCE* pCB_stream_Out;

} CONTEXT;
CONTEXT mContext;

void onSignal(int signal) {
	mContext.isTerminated = true;
}

void* thread_fps(void* data) {
	int trigger_count = 1;
	int num_of_last_captured_frames = 0;

	while(mContext.isValidProcess) {
		int num_of_current_captured_fraems = mContext.frames_captured;
		printf("%c[2K\rFPS : %3d%", 27, num_of_current_captured_fraems - num_of_last_captured_frames);
		num_of_last_captured_frames = num_of_current_captured_fraems;
		int i = 0;
		for(; i < trigger_count; i++) {
			printf("%c", '.');
		}
		for(; i < 6; i++) {
			printf("%c", ' ');
		}
		if(trigger_count++ == 5) {
			trigger_count = 1;
		}
		fflush(stdout);
		usleep(1000 * 1000);
	}
}

void* thread_encoder(void* data) {
	// int fd_demo = open("demo.h264", O_RDWR | O_CREAT | O_TRUNC, 0644);

	struct v4l2_buffer buffer;
	memset(&buffer, 0x00, sizeof(buffer));
	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;

	x264_init(mContext.width, mContext.height, mContext.fps);

	x264_nal_t* nals;
	int num_of_nals;
	x264_get_sps_pps(&nals, &num_of_nals);
	for(int j = 0; j < num_of_nals; j++) {
		// write(fd_demo, nals[j].p_payload, nals[j].i_payload);
		CBput(mContext.pCB_stream_Out, nals[j].p_payload, nals[j].i_payload);
		//printf("NAL TYPE : %02x / %d bytes\n", *(nals[j].p_payload+4), nals[j].i_payload);
		//for(int i = 0; i < nals[j].i_payload; i++) {
		//	printf("%02x", *(nals[j].p_payload+i));
		//}
		//printf("\n\n");
	}

	printf("Start of encoding\n");
	x264_nal_t* p_encoded_frame;
	unsigned char* i420 = malloc(mContext.width * mContext.height * 3 / 2);
	while(mContext.isValidProcess) {
		sem_wait(&mContext.sem_encoder);
		CAPTURE_BUFFER* pBuffer = CQget(mContext.pCQ_encoder_In);
		if(!mContext.isYUV420) {
			convert_yuv422_to_i420(pBuffer->ptr, i420, mContext.width, mContext.height);
		}

		int encoded_frame_size = x264_encode(i420, &p_encoded_frame);
		if(encoded_frame_size) {
			// printf("encoded : %d bytes\n", encoded_frame_size);
			// write(fd_demo, p_encoded_frame->p_payload, encoded_frame_size);
			CBput(mContext.pCB_stream_Out, p_encoded_frame->p_payload, encoded_frame_size);
		}
		buffer.index = pBuffer - mContext.buffer_pool;
		if( -1 == ioctl(mContext.fd_cam, VIDIOC_QBUF, &buffer)) {
			printf("FAIL to queue buffer\n");
			mContext.isValidProcess = false;
		}
	}
	x264_close();
	// close(fd_demo);
}

void* thread_stream(void* data) {
	unsigned char buffer[1024 * 1024];
	if(access(mContext.output_name, F_OK) < 0) {
		mkfifo(mContext.output_name, 0666);
	}
	int fd = open(mContext.output_name, O_WRONLY);
	printf("Broadcast stream : START\n");
	while(mContext.isValidProcess) {
		int nByte = CBget(mContext.pCB_stream_Out, buffer, 1024 * 1024);
		write(fd, buffer, nByte);
	}
	printf("Broadcast stream : FINISH\n");
}

void parse_arguments(int argc, char **argv) {
	char* szDeviceID 	= "/dev/video0";
	char* szOutputFIFO	= "tmp.fifo";

	struct option opt_long[] = {
			{ "width",      required_argument, NULL, 0 },
			{ "height",     required_argument, NULL, 0 },
			{ "fps",	    required_argument, NULL, 0 },
			{ "input",	    required_argument, NULL, 0 },
			{ "output",		required_argument, NULL, 0 },
			{ "YUV420",		no_argument, &mContext.isYUV420, true},
			{ "READWRITE",	no_argument, &mContext.isREADWRITE, true},
			{ "daemon",		no_argument, &mContext.isDaemon, true}
	};

	while(1) {
		int opt_index;
		int opt_char = getopt_long(argc, argv, "w:h:f:i:d", opt_long, &opt_index);
		if(opt_char == -1) {
			break;
		}
		else if(opt_char == 'w' || (opt_char == 0 && opt_index == 0) ) {
			mContext.width = atoi(optarg);
		}
		else if(opt_char == 'h' || (opt_char == 0 && opt_index == 1) ) {
			mContext.height = atoi(optarg);
		}
		else if(opt_char == 'f' || (opt_char == 0 && opt_index == 2) ) {
			mContext.fps = atoi(optarg);
		}
		else if(opt_char == 'i' || (opt_char == 0 && opt_index == 3) ) {
			szDeviceID = optarg;
		}
		else if(opt_char == 'o' || (opt_char == 0 && opt_index == 4) ) {
			szOutputFIFO = optarg;
		}
		else if(opt_char == 'd') {
			mContext.isDaemon = true;
		}
	}

	strcpy(mContext.device_name, szDeviceID);
	strcpy(mContext.output_name, szOutputFIFO);
}


void setExtendedControls(int fd_cam) {
        struct v4l2_control ctrl;

        clear(ctrl);
        ctrl.id = V4L2_CID_EXPOSURE_AUTO;
        ctrl.value = V4L2_EXPOSURE_AUTO;
        ioctl(fd_cam, VIDIOC_S_CTRL, &ctrl);

        clear(ctrl);
        ctrl.id = V4L2_CID_FOCUS_AUTO;
        ctrl.value = true;
        ioctl(fd_cam, VIDIOC_S_CTRL, &ctrl);

        clear(ctrl);
		ctrl.id = V4L2_CID_IMAGE_STABILIZATION;
		ctrl.value = true;
		ioctl(fd_cam, VIDIOC_S_CTRL, &ctrl);

		clear(ctrl);
		ctrl.id = V4L2_CID_ISO_SENSITIVITY_AUTO ;
		ctrl.value = V4L2_CID_ISO_SENSITIVITY_AUTO ;
		ioctl(fd_cam, VIDIOC_S_CTRL, &ctrl);
}


int main(int argc, char **argv) {
	clear(mContext);
	mContext.isValidProcess = true;
	mContext.width  = 640;
	mContext.height = 480;
	mContext.fps    = 15;
	mContext.buffer_pool_size = 5;

	parse_arguments(argc, argv);
	if(mContext.isDaemon) {
		int pid = fork();
		if(pid < 0) {
			exit(EXIT_FAILURE);
		}
		else if(pid > 0) {
			setsid();
			chdir("/");

			// 부모 프로세스라면 PID 기록 후 종료
			char szPID[256];
			sprintf(szPID, "%d\n", pid);
			int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			write(fd, szPID, strlen(szPID) + 1);
			close(fd);
			exit(EXIT_SUCCESS);
		}
	}

	mContext.fd_cam = open_device(mContext.device_name);
	print_capabailty(mContext.fd_cam);
	if(get_video_type(mContext.fd_cam) != V4L2_INPUT_TYPE_CAMERA) {
		printf("Wrong device type has been selected.\n");
		exit(EXIT_FAILURE);
	}
	set_video_format(mContext.fd_cam, mContext.width, mContext.height, mContext.isYUV420 ? V4L2_PIX_FMT_YUV420 : NULL);
	allocate_video_mmap_buffer(mContext.fd_cam, &mContext.buffer_pool_size, &mContext.buffer_pool);
	mContext.fps = (int)set_video_fps(mContext.fd_cam, mContext.fps);
	prepare_video_mmap_buffer(mContext.fd_cam, mContext.buffer_pool_size);
	setExtendedControls(mContext.fd_cam);
	video_stream_on(mContext.fd_cam);

	// Streamer On
	mContext.pCB_stream_Out = CBcreateInstance(1024 * 1024 * 4);
	pthread_t pt_stream;
	pthread_create(&pt_stream, NULL, thread_stream, NULL);

	// Encoder on
	mContext.pCQ_encoder_In = CQcreateInstance(mContext.buffer_pool_size);
	sem_init(&mContext.sem_encoder, 0, 0);
	pthread_t pt_encoder;
	pthread_create(&pt_encoder, NULL, thread_encoder, NULL);

	// Frame rate measurer on
	// pthread_t pt_fps;
	// pthread_create(&pt_fps, NULL, thread_fps, NULL);


	// Set signal interrupt handler
	signal(SIGINT, 	onSignal);
	signal(SIGTSTP, onSignal);
	signal(SIGTERM, onSignal);

	// for(int i = 0; mContext.isValidProcess && i < 100; i++) {
	while(!mContext.isTerminated) {
		// Stream 대기
		fd_set selector;
		FD_ZERO(&selector);
		FD_SET(mContext.fd_cam, &selector);

		struct timeval selector_timer;
		selector_timer.tv_sec = 2;
		selector_timer.tv_usec = 0;

		int r = select(mContext.fd_cam + 1, &selector, NULL, NULL, &selector_timer);
		if( r == -1 ) {
			if(errno == EINTR) {
				printf("Interrupted by system.\n");
			}
			else {
				printf("Got fucked.\n");
				mContext.isValidProcess = false;
			}
			continue;
		}
		else if( r == 0 ) {
			printf("Time out. skip this frame.\n");
			continue;
		}

		struct v4l2_buffer buffer;
		unsigned int index;

		memset(&buffer, 0x00, sizeof(buffer));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buffer.memory = V4L2_MEMORY_MMAP;

		if( -1 == ioctl(mContext.fd_cam, VIDIOC_DQBUF, &buffer)) {
			switch(errno) {
			case EAGAIN :
				printf("Retry.\n");
				continue;
				break;
			case EIO :
				printf("EIO. Skip frame.\n");
				break;
			default :
				printf("FUCK!\n");
				mContext.isValidProcess = false;
				continue;
			}
		}
		mContext.frames_captured++;

		CQput(mContext.pCQ_encoder_In, mContext.buffer_pool + buffer.index);
		sem_post(&mContext.sem_encoder);
	}

	signal(SIGINT, 	SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

	mContext.isValidProcess = false;
//	pthread_join(pt_fps, NULL);

	sem_post(&mContext.sem_encoder);
	pthread_join(pt_encoder, NULL);
	sem_close(&mContext.sem_encoder);
	CQdestroy(mContext.pCQ_encoder_In);

	pthread_join(pt_stream, NULL);
	CBdestroy(mContext.pCB_stream_Out);

	video_stream_off(mContext.fd_cam);
	release_video_mmap_buffer(mContext.fd_cam, mContext.buffer_pool, mContext.buffer_pool_size);
	close(mContext.fd_cam);
}
