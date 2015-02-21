#include "x264sonien.h"

x264_param_t   param;
x264_picture_t picture;
x264_t*        handle = NULL;
int            num_of_frame = 0;

int WIDTH, HEIGHT, FPS;
int x264_init(int width, int height, int fps) {
	WIDTH  = width;
	HEIGHT = height;
	FPS    = fps;

//	if(... < 0) return -1;
//	x264_param_default_preset( &param, "medium", NULL )
//	x264_param_default_preset(&param, "veryfast", "zerolatency")
	if(x264_param_default_preset(&param, "ultrafast", "zerolatency") < 0) return -1;
	// x264_param_default(&param);
	param.i_width     = width;
	param.i_height    = height;
	param.i_fps_num   = fps;
	param.i_fps_den   = 1;
	param.b_annexb    = 1;
	param.i_keyint_max= 2 * fps;
	if(NUM_OF_THREAD) {
		param.i_threads = NUM_OF_THREAD;
	}

	if(x264_param_apply_profile( &param, "baseline") < 0)
		return -2;

	if(x264_picture_alloc(&picture, param.i_csp, param.i_width, param.i_height) < 0)
		return -3;

	handle = x264_encoder_open(&param);
	if(!handle) return -4;

	return 0;
}


int x264_get_sps_pps(x264_nal_t** nals, int* num_of_nals) {
	if(!handle) return -1;
	return x264_encoder_headers(handle, nals, num_of_nals);
}

int x264_encode(unsigned char* yuv, x264_nal_t** pp_nal) {
	*pp_nal = NULL;

	x264_nal_t*    nals;
	int            num_of_nals;
	x264_picture_t picture_encoded;

	picture.img.plane[0] = yuv;
	picture.img.plane[1] = yuv + WIDTH * HEIGHT;
	picture.img.plane[2] = yuv + WIDTH * HEIGHT * 5 / 4;
	picture.i_pts = num_of_frame++;

	int frame_size = x264_encoder_encode(handle, &nals, &num_of_nals, &picture, &picture_encoded);
	if(frame_size < 0)
		return -1;

	*pp_nal = &nals[0];
	return frame_size;
}

void x264_close() {
	x264_encoder_close(handle);
	x264_picture_clean(&picture);
}
