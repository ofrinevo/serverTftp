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

#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5


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
	short opcode = OPCODE_DATA;
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
	short opcode = OPCODE_ACK;
	if (sprintf(buf, "%hd%hd", opcode, blockNumber) < 0) {
		perror("sprintf");
		return -1;
	}
	sendto(sockfd, &buf, SIZE + 4, 0, dest_adrr, addrLen);
	return 0;
}

int sendError(short errorCode, char* errMsg,int sizeMsg) {
	char buf[200];
	if (sprintf(buf, "%lh%s0", errorCode, errMsg) < 0) {
		perror("sprintf");
		return -1;
	}
	//TODO actual send
	return 0;
}


readSize recvData(FILE* fp,char* buf, int sockfd, const struct sockaddr *dest_adrr, socklen_t addrLen) {
	//TODO
}

int handleRequest(short* opcode, char* bufRecive, char** fileName) {
	if (sscanf(bufRecive, "%hd0%s", *opcode, *fileName) != 2) {
		perror("sscanf");
		return -1;
	}
	return 0;
}


int init_server() {
	struct sockaddr_in myaddr, remaddr;
	
	socklen_t addrlen = sizeof(remaddr);
	int sockfd;
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
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
	return 0;
}



int main() {

	return 0;
}