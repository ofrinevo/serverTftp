#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <err.h>
#include <time.h>

#define PORT 69
#define TRUE 1
#define FALSE 0
#define DEBUG 1


int init_server() {
	struct sockaddr_in myaddr, remaddr;
	
	socklen_t addrlen = sizeof(remaddr);
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Error creating socket\n");
		return -1;
	}
	memset((char*)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_MY);
	myaddr.sin_port = htons(PORT);
	if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("Error binding\n");
		return -1;
	}
	return sockfd;
}

int sendData()
{
 
}

int recvData() {
	//recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen);
}


int main() {
	//server loop:
	int cntRead, cntWrite;
	int sockfd = init_server();
	if (sockfd < 0)
		return 0; //error- terminating program!


	

	while (TRUE) {
		/*	read from client
			look for op code
			see if legal
			send ack msg
		*/
	}
	return 0;
}