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
#define SIZE 512
#define PORT 69
#define TRUE 1
#define FALSE 0
#define DEBUG 1

typedef struct readSize {
	short blockNumber;
	int sizeRead;
} readSize;

//return num of bytes read from the file on success, -1 else
int sendData(FILE* fp,short blockNumber,int sockfd,const struct sockaddr *dest_adrr,socklen_t addrLen){
	if (fseek(fp, blockNumber*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE+4];
	short opcode = 3;
	if (sprintf(buf, "%hd%hd", opcode, blockNumber)) {
		perror("sprintf");
		return -1;
	}
		
	int sizeRead=fread(buf, SIZE, 1, fp);
	if (ferror(fp)) {
		perror("fread");
		return -1;
	}
	sendto(sockfd, &buf, SIZE + 4, 0, dest_adrr, addrLen);
	return sizeRead;
}

//returns 0 on success, -1 else
int sendAck(short blockNumber, int sockfd, const struct sockaddr *dest_adrr, socklen_t addrLen) {
	char buf[4];
	short opcode = 4;
	if (sprintf(buf, "%hd%hd", opcode, blockNumber)) {
		perror("sprintf");
		return -1;
	}
	sendto(sockfd, &buf, SIZE + 4, 0, dest_adrr, addrLen);
	return 0;
}


readSize recvData(FILE* fp,char* buf, int sockfd, const struct sockaddr *dest_adrr, socklen_t addrLen) {
	//TODO
}




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