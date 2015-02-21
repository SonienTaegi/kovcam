#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LENGTH 100

bool isValid = true;
void onSignal(int signal) {
	isValid = false;
	printf("\r");
}

int main() {
	unsigned char data[LENGTH];
	memset(data, 0x00, LENGTH);
	data[0]     = 0xAA;
	data[LENGTH - 1] = 0x55;

	signal(SIGINT, 	onSignal);
	signal(SIGTSTP, onSignal);
	signal(SIGTERM, onSignal);
	signal(SIGPIPE, onSignal);

//	mkfifo("tmp.fifo", 0644);
	int fd = open("/tmp/kovcam.fifo", O_WRONLY);
	unsigned char index = 0;
	while(isValid) {
		data[1] = index++;
		write(fd, data, LENGTH);
		printf("진리콩까네\n");
		usleep(1000 * 1000);
	} 
	close(fd);
//	unlink("tmp.fifo");

	printf("안녕 진리콩!\n");
}
