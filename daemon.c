#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <getopt.h>             /* getopt_long() */

#define FIFO_FILE	"/tmp/kovcam.fifo"

bool isKilling = false;
bool isValidProcess = true;
int width 	= 1280;
int height 	= 720;
int fps		= 10;

void parse_arguments(int argc, char **argv) {
	struct option opt_long[] = {
			{ "width",      required_argument, NULL, 0 },
			{ "height",     required_argument, NULL, 0 },
			{ "fps",	    required_argument, NULL, 0 },
			{ "kill",		no_argument, &isKilling, true},
	};

	while(1) {
		int opt_index;
		int opt_char = getopt_long(argc, argv, "w:h:f:k", opt_long, &opt_index);
		if(opt_char == -1) {
			break;
		}
		else if(opt_char == 'w' || (opt_char == 0 && opt_index == 0) ) {
			width = atoi(optarg);
		}
		else if(opt_char == 'h' || (opt_char == 0 && opt_index == 1) ) {
			height = atoi(optarg);
		}
		else if(opt_char == 'f' || (opt_char == 0 && opt_index == 2) ) {
			fps = atoi(optarg);
		}
		else if(opt_char == 'k' || (opt_char == 0 && opt_index == 3) ) {
			isKilling = true;
		}
	}
}

void kill_process(unsigned char* pidFile) {
	if(access(pidFile, F_OK) == 0) {
		unsigned char strPID[1024];
		int fd = open(pidFile);
		read(fd, strPID, 1024);
		close(fd);

		kill(atoi(strPID), 9);
	}
}

int main(int argc, char **argv) {
	parse_arguments(argc, argv);

	kill_process("/tmp/kovcam.camera.pid");
	kill_process("/tmp/kovcam.broadcaster.pid");
	kill_process("/tmp/kovcam.was.pid");
	if(isKilling) {
		exit(EXIT_SUCCESS);
	}


	if(access(FIFO_FILE, F_OK) == 0) {
		remove(FIFO_FILE);
	}

	mkfifo(FIFO_FILE, 0666);
	unsigned char cmd[1024];

	memset(cmd, 0x00, 1024);
	sprintf(cmd, "./camera/kovcam -d -w%d -h%d -f%d --output %s", width, height, fps, FIFO_FILE);
	system(cmd);

	memset(cmd, 0x00, 1024);
	sprintf(cmd, "python3 ./broadcaster/kovcam.py %d %d %d %s", width, height, fps, FIFO_FILE);
	system(cmd);

	exit(EXIT_SUCCESS);
}
