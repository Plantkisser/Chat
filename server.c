#define _GNU_SOURCE

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "Connection.h"




enum constants {
	BUFSIZE 		= 100,
	STD_PROTOCOL 	= 0,
	QUEUE_LEN 		= 30, 
	NO_FLAGS 		= 0,
	ENABLE 			= 1,
	DISABLE 		= 0,
	MAX_CLIENTS		= 100000
};

enum semaphore {
	SEM_OWN 		= 0, // main semaphore is needed to edit client_inf
	SEM_ADD_ENABLE 	= 1  // when equals 1 new clients is able to add sockets in client_inf, it equals 0 mean that only warden able to get permission to client_inf  
};





struct sck_inf
{
	int sck;
	char enbl;
	int id;
};

struct client_inf
{
	struct sck_inf client_list[MAX_CLIENTS];
	int cl_count;
	int next_cl_id;
};

struct add_inf
{
	int semid;
	int cl_sck;

	struct client_inf* p_cl;
};

struct warden_inf
{
	int semid;
	struct client_inf* p_cl;
};




void* commands(void* data)  
{
	int* p_lsn_sck = data;

	char buf[BUFSIZE];
	while(1) {
		memset(buf, 0, BUFSIZE);
		scanf("%s", buf);
		if (strcmp("stop", buf) == 0) {
			unlink(SUN_PATH);
			close(*p_lsn_sck);
			exit(0);
		}
	}
}




void* add_to_chat(void* data)
{
	struct add_inf* s_add = data;

	struct sembuf sops[3];
	sops[0].sem_num 	= SEM_OWN;
	sops[0].sem_op		= -1;
	sops[0].sem_flg		=  0;
	sops[1].sem_num 	= SEM_ADD_ENABLE;
	sops[1].sem_op		= -1;
	sops[1].sem_flg		=  0;
	sops[2].sem_num 	= SEM_ADD_ENABLE;
	sops[2].sem_op		=  1;
	sops[2].sem_flg		=  0;

	semop(s_add->semid, sops, 3);

	int i = 0;
	for (i = 0; i < MAX_CLIENTS; ++i) {
		if (s_add->p_cl->client_list[i].enbl == DISABLE)
		{
			s_add->p_cl->cl_count++;
			s_add->p_cl->client_list[i].enbl = ENABLE;
			s_add->p_cl->client_list[i].sck = s_add->cl_sck;
			s_add->p_cl->client_list[i].id = s_add->p_cl->next_cl_id++;
			break;
		}
	}

	sops[0].sem_num 	= SEM_OWN;
	sops[0].sem_op		=  1;
	sops[0].sem_flg		=  0;

	semop(s_add->semid, sops, 1);
	printf("Accepted successfully\n");

	return NULL;
}


int update_set(fd_set* set, struct client_inf* p_cl)
{
	FD_ZERO(set);
	int i = 0;
	int j = 0;
	int maxfd = 0;
	int cl_count = p_cl->cl_count;
	for (i = 0, j = 0; j < cl_count; ++i) {
		if (p_cl->client_list[i].enbl == ENABLE) {
			maxfd = (maxfd > p_cl->client_list[i].sck) ? maxfd : p_cl->client_list[i].sck;
			FD_SET(p_cl->client_list[i].sck, set);
			++j;
		}
	}
	return maxfd;
}

int send_message(struct client_inf* p_cl, int num, char* msg, int len, int cl_id)
{
	if (send(p_cl->client_list[num].sck, &cl_id, sizeof(int), NO_FLAGS) == -1 || 
		  send(p_cl->client_list[num].sck, &len, sizeof(int), NO_FLAGS) == -1 || send(p_cl->client_list[num].sck, msg, len, NO_FLAGS) == -1) {
		p_cl->cl_count--;
		p_cl->client_list[num].enbl = DISABLE;
	}
}

char* receive_message(int sck, int* len)
{
	if (recv(sck, len, sizeof(int), NO_FLAGS) <= 0) {
		*len = 0;
		return NULL;
	}
	char* msg = (char*) calloc(*len + 1, sizeof(char));
	if (msg == NULL) {
		perror("calloc in receive receive_message");
		exit(EXIT_FAILURE);
	}

	if (recv(sck, msg, *len, NO_FLAGS) <= 0) {
		*len = 0;
		return NULL;
	}
	msg[*len] = '\0';

	return msg;
}

void* warden(void* data)
{
	struct warden_inf* p_ward_inf = data; 
	
	struct sembuf sops[2];
	sops[0].sem_num 	= SEM_ADD_ENABLE;
	sops[0].sem_op		= -1;
	sops[0].sem_flg		=  0;
	semop(p_ward_inf->semid, sops, 1);

	sops[0].sem_num 	= SEM_OWN;
	sops[0].sem_op		= -1;
	sops[0].sem_flg		=  0;
	semop(p_ward_inf->semid, sops, 1);

	fd_set set;

	int i = 0;
	int j = 0;
	int cl_count = p_ward_inf->p_cl->cl_count;

	int maxfd = update_set(&set, p_ward_inf->p_cl);

	sops[0].sem_num 	= SEM_OWN;
	sops[0].sem_op		=  1;
	sops[0].sem_flg		=  0;
	sops[1].sem_num 	= SEM_ADD_ENABLE;
	sops[1].sem_op		=  1;
	sops[1].sem_flg		=  0;
	semop(p_ward_inf->semid, sops, 2);

	struct timeval tvl;
	tvl.tv_sec = 2;
	tvl.tv_usec = 500000;

	int res = 0;
	while (1) {
		tvl.tv_sec = 2;
		tvl.tv_usec = 500000;
		res = select(maxfd + 1, &set, NULL, NULL, &tvl);
		if (res == -1) {
			perror("select");
			sleep(5);
			exit(EXIT_FAILURE);
		}

		sops[0].sem_num 	= SEM_ADD_ENABLE;
		sops[0].sem_op		= -1;
		sops[0].sem_flg		=  0;
		semop(p_ward_inf->semid, sops, 1);

		sops[0].sem_num 	= SEM_OWN;
		sops[0].sem_op		= -1;
		sops[0].sem_flg		=  0;
		semop(p_ward_inf->semid, sops, 1);


		for (i = 0, j = 0; j < res; ++i) {
			if (p_ward_inf->p_cl->client_list[i].enbl == ENABLE && FD_ISSET(p_ward_inf->p_cl->client_list[i].sck, &set)) {
				++j;
				char* msg = NULL;
				int len = 0;
				msg = receive_message(p_ward_inf->p_cl->client_list[i].sck, &len);

				if (msg == NULL) {
					printf("Client is dead id: %d\n", p_ward_inf->p_cl->client_list[i].id);
					close(p_ward_inf->p_cl->client_list[i].sck);
					p_ward_inf->p_cl->client_list[i].enbl = DISABLE;
					p_ward_inf->p_cl->cl_count--;
					continue;
				}
				int cl_id = p_ward_inf->p_cl->client_list[i].id;
				int k = 1;
				int l = 0;
				cl_count = p_ward_inf->p_cl->cl_count;
				for (k = 1, l = 0; k < cl_count; l++) {
					if (p_ward_inf->p_cl->client_list[l].enbl == ENABLE && l != i) {
						send_message(p_ward_inf->p_cl, l, msg, len, cl_id);
						k++;
					}
				}
				cl_count = p_ward_inf->p_cl->cl_count;

				free(msg);
			}
		}

		maxfd = update_set(&set, p_ward_inf->p_cl);

		sops[0].sem_num 	= SEM_ADD_ENABLE;
		sops[0].sem_op		=  1;
		sops[0].sem_flg		=  0;
		sops[1].sem_num 	= SEM_OWN;
		sops[1].sem_op		=  1;
		sops[1].sem_flg		=  0;
		semop(p_ward_inf->semid, sops, 2);
	} 
}

int create_and_init_sem()
{
	int semid = semget(IPC_PRIVATE, 2, 0666);

	struct sembuf sops[2];
	sops[0].sem_num 	= SEM_ADD_ENABLE;
	sops[0].sem_op		=  1;
	sops[0].sem_flg		=  0;
	sops[1].sem_num 	= SEM_OWN;
	sops[1].sem_op		=  1;
	sops[1].sem_flg		=  0;
	semop(semid, sops, 2);

	return semid;
}


int block_sig(int signum)
{
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, signum);
	sigprocmask(SIG_BLOCK, &mask, NULL);
	return 0;
}

int create_listen_socket()
{
	int optval_true = 1;

	int lsn_sck = socket(AF_UNIX, SOCK_STREAM, STD_PROTOCOL);
	if (lsn_sck == -1) {
		perror("socket in function create_listen_socket");
		exit((EXIT_FAILURE));
	}

	struct sockaddr_un lsn_addr;
	memset(&lsn_addr, 0, sizeof(lsn_addr));
	lsn_addr.sun_family = AF_UNIX;
	strncpy(lsn_addr.sun_path, SUN_PATH, sizeof(lsn_addr.sun_path) - 1);

	if  (bind(lsn_sck, &lsn_addr, sizeof(struct sockaddr_un)) != 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	return lsn_sck;
}

int init_client_inf(struct client_inf* clnt_inf)
{
	clnt_inf->cl_count = 0;
	clnt_inf->next_cl_id = 1;
	int i = 0;
	for (i = 0; i < MAX_CLIENTS; ++i) {
		clnt_inf->client_list[i].enbl = DISABLE;
	}
}


int main() 
{
	block_sig(SIGPIPE);

	struct client_inf clnt_inf;
	init_client_inf(&clnt_inf);

	int semid = create_and_init_sem();

	int lsn_sck = create_listen_socket();

	if (listen(lsn_sck, QUEUE_LEN) != 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	pthread_t tmp_thr;
	if (pthread_create(&tmp_thr, NULL, commands, &lsn_sck) != 0) {
		perror("commands pthread_create");
		exit(EXIT_FAILURE);
	}


	struct warden_inf w_inf;
	w_inf.semid = semid;
	w_inf.p_cl = &clnt_inf;
	if (pthread_create(&tmp_thr, NULL, warden, &w_inf) != 0) {
		perror("warden pthread_create");
		exit(EXIT_FAILURE);
	}


	while(1) {
		int sockfd = accept(lsn_sck, NULL, 0);
		if (sockfd == -1) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		struct add_inf* p_add_inf =(struct add_inf*) calloc(1, sizeof(struct add_inf));
		p_add_inf->cl_sck = sockfd;
		p_add_inf->semid = semid;
		p_add_inf->p_cl = &clnt_inf;

		if (pthread_create(&tmp_thr, NULL, add_to_chat, p_add_inf) != 0) {
			perror("add_to_chat pthread_create");
			close(p_add_inf->cl_sck);
			free(p_add_inf);
		}
	}

	close(lsn_sck);

	return 0;
}