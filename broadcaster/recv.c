#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

bool isValid = true;
void onSignal(int signal) {
	isValid = false;
	printf("\r");
}

int main() {
	signal(SIGINT, 	onSignal);
	signal(SIGTSTP, onSignal);
	signal(SIGTERM, onSignal);

	unsigned char buffer[65536];
	int fd = open("tmp.fifo", O_RDONLY | O_NONBLOCK);
	if(fd < 0 ) {
		printf("아니 내가 고자라니!\n");
		exit(-1);
	}

	while(isValid) {
		int size = read(fd, buffer, 65535);
		if(size <= 0) {
			printf("READ : DUMM\n");
			usleep(1000*1000);
			continue;
		}

		buffer[size] = 0x00;
		printf("READ : %s\n", buffer);
	} 
	close(fd);

	printf("안녕 진리콩!\n");
}
