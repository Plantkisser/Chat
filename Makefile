all:
	gcc -o serv -pthread server.c
	gcc -o client -pthread client.c