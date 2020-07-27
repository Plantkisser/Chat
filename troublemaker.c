#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#define FILE_NAME_IN "/tmp/909jhsfihIN"
#define FILE_NAME_OUT "/tmp/909jhsfihgOUT"

int main(int argc, char* argv[])
{
	if (argc != 2) {
		printf("Wrong argumets\n");
		exit(0);
	}

	int n = strtol(argv[1], NULL, 10);

	creat(FILE_NAME_IN, 0666);
	creat(FILE_NAME_OUT, 0666);
	int fd = open(FILE_NAME_IN, O_TRUNC | O_WRONLY);
	char buf[100];
	strcpy(buf, "./client < ");
	strcat(buf, FILE_NAME_IN);
	strcat(buf, " > ");
	strcat(buf, FILE_NAME_OUT);


	int i = 0;
	for (i = 0; i < n; ++i) {
		if (fork() == 0) {
			system(buf);
			return 0;
		}
	}

	char command[100];
	int oldflags = fcntl(0, F_GETFL);
	fcntl(0, F_SETFL, oldflags | O_NONBLOCK);
	while(1) {
		read(0, command, 100);
		if (strcmp(command, "stop\n") == 0) {
			sleep(7);
			write(fd, "__END", strlen("__END"));
			fcntl(0, F_SETFL, oldflags & ~O_NONBLOCK);
			return 0;
		}

		write(fd, "Still alive", strlen("Still alive"));
		sleep(2);
	}

	sleep(3);

	return 0;
}