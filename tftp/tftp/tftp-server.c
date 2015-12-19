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
#include <sys/time.h>

#define SIZE 512
#define PORT 69
#define TRUE 1
#define FALSE 0
#define DEBUG 1
short state = -1;
#define OP_CODE= sizeof(short);
struct timeval timeout;

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
int sendData(FILE* fp, short blockNumber, int sockfd, const struct sockaddr *dest_adrr, socklen_t addrLen) {
	if (fseek(fp, blockNumber*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE + 4];
	short opcode = OPCODE_DATA;
	if (sprintf(buf, "%hd%hd", opcode, blockNumber)) {
		perror("sprintf");
		return -1;
	}

	int sizeRead = fread(buf, SIZE, 1, fp);
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

int sendError(short errorCode, char* errMsg, int sizeMsg) {
	char buf[200];
	if (sprintf(buf, "%lh%s0", errorCode, errMsg) < 0) {
		perror("sprintf");
		return -1;
	}
	//TODO actual send
	return 0;
}


// returns number of bytes read on success, -1 on error
//Checks if opcode and blocknumber are correct
//If so, updates the buf to contain the data (only in DATA case)
//on time out returns -3
int receive_message(int s, char* buf, struct sockaddr_in* source, short opcode, short blockNumber) {
	socklen_t fromlen = sizeof(struct sockaddr_in);
	char bufRecive[518];
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));
	int received = recvfrom(s, buf, SIZE, 0, (struct sockaddr*)source, &fromlen);
	if (received == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return -3;
		else
			return -1;
	}
	short currBlockNum, currOpCode;
	sscanf(bufRecive, "%hd%hd%s", currOpCode, currBlockNum, buf);
	if (currBlockNum != blockNumber)
		return -2;
	if (currOpCode != opcode)
		return -2;
	return received;
}

short getOpCode(char* buf) {
	short op;
	if (sscanf(buf, "%hd", op) < 0) {
		perror("sscanf");
		return -1;;
	}
	return op;
}

int handleFirstRequest(char* bufRecive) {
	short opcode;
	char* fileName;
	struct stat fdata;
	if (sscanf(bufRecive, "%hd0%s", opcode, fileName) != 2) {
		perror("sscanf");
		return -1;
	}
	if (opcode == OPCODE_RRQ) {
		if (stat(fileName, &fdata) != 0){
			if (errno == ENOENT || errno == ENOTDIR){
				//TODO error
			}
		}
		if (S_ISDIR(fdata.st_mode)){
			//TODO error
		}
		file = fopen(fileName, O_RDONLY);
	}
	else {

		// check if the file already exists
		if (stat(fileName, &fdata) == 0){
			//TODO error
		}
		// ENOENT means there is no file, otherwise there is a problem
		if (errno != ENOENT){
			//TODO error
		}

		// open the file
		file = open(g_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (file == -1){
			//ERROR
		}
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

int handleWriting(char* buf) {
	char toWrite[518];
	short op, block;
	sscanf(buf, "%hd%hd%s", op, block, toWrite);
	int len = strlen(toWrite);
	int write = fwrite(toWrite, 1, len, file);
	if (write != len) {
		perror("write");
		return -1;
	}
	return 0;
}

int handleReading(char* buf,int blockNumber) {
	sendData()
}

/*return function that we need to use..
	return values- think about this later..*/
int handle(short op, char* buf, struct sockaddr_in* source) {
	if (clientSocket != 0 && addrcmp(source, &clientSocket)) {
		//send error- unknown client!
	}
	if (op == OPCODE_RRQ || op == OPCODE_WRQ) {
		if (state != -1)
			return -2;
		if (op == OPCODE_RRQ) {
			if (state == -1)
				state = OPCODE_RRQ;
		}
		else if (op == OPCODE_WRQ) {
			if (state == -1)
				state = OPCODE_RRQ;
		}
		return handleFirstRequest(buf);
	}
	else if (op == OPCODE_DATA) {
		if (state == OPCODE_RRQ || state == -1)
			return -2;
		else
			//
	}
}


int main(int argc, char* argv[]) {
	//server loop:
	int cntRead, cntWrite;
	clientSocket = 0;
	int sockfd = init_server();
	if (sockfd < 0) {
		perror("init server");
		return 0; //error- terminating program!
	}
	int blockNumber = 0;
	struct stat statbuf;
	char buf[512];
	char fileName[100];
	short op;
	struct sockaddr_in source;
	int recv;
	int func;
	int errCode;
	struct timeval time = { 3,0 };

	while (TRUE) {
		recv = receive_message(clientSocket == 0 ? sockfd : clientSocket, buf, &source,);
		if (recv < 0) {
			//handle..
		}
		op = getOpCode(buf);

		func = handle(op, buf, &source);
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