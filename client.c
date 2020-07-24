#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "Connection.h"




enum constants {
	BUFSIZE = 100,
	NO_FLAGS = 0,
	STD_PROTOCOL = 0,
	O_FLAGS = 0,
	STDIN = 0
};


void* recv_routine(void* data)
{
	int* mn_sck = data;
	int len = 0;

	while(1) {
		int id = 0;
		if (recv(*mn_sck, &id, sizeof(int), NO_FLAGS) <= 0) {
			printf("RECServer is dead\n");
			exit(0);
		}


		if (recv(*mn_sck, &len, sizeof(int), NO_FLAGS) <= 0) {
			printf("RECServer is dead\n");
			exit(0);
		}
		char* buf = (char*) calloc(len + 1, sizeof(char));

		if (recv(*mn_sck, buf, len, NO_FLAGS) <= 0) {
			printf("RECServer is dead\n");
			exit(0);
		}
		buf[len] = '\0';

		printf("%d: %s\n", id, buf);
	}
}

int main() 
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGPIPE);
	sigprocmask(SIG_BLOCK, &mask, NULL);

	struct sockaddr_un serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, SUN_PATH, sizeof(serv_addr.sun_path) - 1);

	int mn_sck = socket(AF_UNIX, SOCK_STREAM, STD_PROTOCOL);
	if (mn_sck == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	if (connect(mn_sck, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
		perror("connect");
		exit(EXIT_FAILURE);
	}



	char buf[BUFSIZE + 1];
	memset(buf, 0, BUFSIZE + 1);

	printf("Warning: %d characters is a maximum length in one messages\n", BUFSIZE);
	pthread_t tmp_thr;

	if (pthread_create(&tmp_thr, NULL, recv_routine, &mn_sck) != 0) {
		perror("recv pthread_create");
		exit(EXIT_FAILURE);
	}

	while(1) {
		int len = read(STDIN, buf, BUFSIZE);
		len--;
		buf[len] = '\0';
		if (strcmp(buf, "__END") == 0) {
			close(mn_sck);
			exit(0);
		}

		if (send(mn_sck, &len, sizeof(len), NO_FLAGS) == -1 || send(mn_sck, buf, strlen(buf), NO_FLAGS) == -1) {
			printf("Server is dead\n");
			exit(0);
		}
	}
	return 0;
}