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

#define OP_CODE= sizeof(short);


#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5

int clientSocket;
FILE* file = NULL;

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

//if you are reading this, we need function that getting commangds from client, not neccerly using FILE- i did it
// look 1 function below this.. 
readSize recvData(FILE* fp,char* buf, int sockfd, const struct sockaddr *dest_adrr, socklen_t addrLen) {
	//TODO
}

// returns number of bytes read on success, -1 on error
int receive_message(int s, char buf[512], struct sockaddr_in* source)
{
	socklen_t fromlen = sizeof(struct sockaddr_in);
	int received = recvfrom(s, buf, 512, 0,
		(struct sockaddr*)source, &fromlen);
	return received;
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
		close(sockfd);
		return -1;
	}
	return sockfd;
}

static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2){
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

/*return function that we need to use..
	return values- think about this later..*/
int function(short op, char fileName[100], struct sockaddr_in* source) {
	if (clientSocket != 0 && addrcmp(source, &clientSocket)){
		//send error- unknown client!
	}
	if (op != OPCODE_RRQ && op != OPCODE_WRQ)
	{
		//send error msg..
	}

	if (strstr(fileName, "/") != NULL || strstr(fileName, "\\") != NULL)
	{
		//error msg..
	}

	if (op == OPCODE_RRQ)
	{
		//RRQ msg
	}
	if (op == OPCODE_WRQ)
	{
		//WRQ msg
	}
}


int main() {
	//server loop:
	int cntRead, cntWrite;
	clientSocket = 0;
	int sockfd = init_server();
	if (sockfd < 0)
		return 0; //error- terminating program!
	int blockNumber = 0;
	struct stat statbuf;
	char buf[512];
	char fileName[100];
	short op;
	struct sockaddr_in source;
	int recv;
	int func;
	int errCode;

	while (TRUE) {
		recv = receive_message(clientSocket==0 ? sockfd:clientSocket, buf, &source );
		if (recv < 0) {
			//handle..
		}
		handleRequest(&op, buf, fileName);
		//send to function to decide what do to next..
		func= function(op, fileName, &source);
		// if func tell us to read:
		sendData(file, blockNumber, sockfd, &source, sizeof(source));
		// and update block number if needed!
		// if write- write
		//...........
		//send error if needed:
		//sendError(errCode, errorMsg(create one or preDefine), size of errorMsg )
		
		//send ack if needed:
		//sendAck(blockNumber, sockfd, &source, sizeof(source));
		
		//close client if finish and assign - clientSoket=0;
		// start serving other clients
	}
	return 0;
}