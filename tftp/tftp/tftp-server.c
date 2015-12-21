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
#include <stdint.h>
#include <sys/stat.h>

#define ERRDESC_UNKNOWN_TID \
	"Unknown transfer ID."
#define ERRDESC_UNEXPECTED_OPCODE_DURING_IDLE \
	"Received an unexpected opcode while expecting RRQ or WRQ"
#define ERRDESC_UNEXPECTED_OPCODE_DURING_READ_IN_PROGRESS \
	"Received an unexpected opcode while expecting ACK"
#define ERRDESC_UNEXPECTED_OPCODE_DURING_WRITE_IN_PROGRESS \
	"Received an unexpected opcode while expecting DATA"
#define ERRDESC_RQ_FILE_NOT_FOUND \
		"File not found."
#define ERRDESC_ALLOC_EXCEEDED \
		"Disk full or allocation exceeded."
#define ERRDESC_RQ_FILE_IS_A_DIRECTORY \
		"Requested file is a directory"
#define ERRDESC_WRQ_FILE_ALREADY_EXISTS \
		"File to be written already exists"
#define ERRDESC_WRQ_UNABLE_TO_CREATE_FILE \
		"Unable to create the new file"
#define ERRDESC_RRQ_ACCESS_VIOLATION \
		"Unable to access the requested file"
#define ERRDESC_OPEN_FAILED \
		"Unable to open requested file"
#define ERRDESC_STAT_FAILED \
		"Unable to stat requested file"
#define ERRDESC_READ_FAILED \
		"Unable to read from requested file"
#define ERRDESC_WRITE_FAILED \
		"Unable to write to the requested file"
#define ERRDESC_ACK_BLOCK_MISMATCH \
		"ACKed block number was not as expected"
#define ERRDESC_DATA_BLOCK_MISMATCH \
		"Data block number was not as expected"
#define ERRDESC_INTERNAL_ERROR \
		"Internal server error."

#define SIZE 512
#define PORT 57983
#define TRUE 1
#define FALSE 0
#define DEBUG 1
short state = -1;
#define OP_CODE = sizeof(short);
struct timeval timeout;
struct sockaddr_in client;

#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5



int blockNumber;
int clientSocket;
int serverSocket;
FILE* file = NULL;

typedef struct readSize {
	short blockNumber;
	int sizeRead;
} readSize;



//return 1 on success, -1 otherwise
int init_client()
{
	struct sockaddr_in servaddr;
	int new_socket = socket(AF_INET, SOCK_DGRAM, 0);

	if (new_socket == -1)
	{
		printf("Error: socket() failed: %s\n", strerror(errno));
		return -1;
	}
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(0);


	if (bind(new_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("Error: bind() failed: %s\n", strerror(errno));
		return -1;
	}
	if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		printf("Error: setsockopt() failed: %s\n", strerror(errno));
		return -1;
	}

	clientSocket = new_socket;
	return 0;
}


int close_client() {
	int socket = clientSocket;

	if (socket == 0)
		return 1;

	clientSocket = 0;
	return close(socket);
}

//return num of bytes read from the file on success, -1 else
int sendData(const struct sockaddr_in *dest_adrr) {
	printf("got here1\n");
	if (fseek(file, blockNumber*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE + 4];
	printf("got here1\n");
	short opcode = htons(OPCODE_DATA);
	short blkTons = htons(blockNumber);
	((uint16_t*)buf)[0] = opcode;
	((uint16_t*)buf)[1] = blkTons;
	
	printf("got here1\n");
	int sizeRead = fread(buf+4, SIZE, 1, file);
	if (ferror(file)) {
		perror("fread");
		return -1;
	}
	sendto(clientSocket, &buf, SIZE + 4, 0, (struct sockaddr*)dest_adrr,
		sizeof(struct sockaddr_in));
	return sizeRead;
}

//returns 0 on success, -1 else
int sendAck(const struct sockaddr_in *dest_adrr) {
	uint16_t buf[2];
	uint16_t opcode = htons(OPCODE_ACK);
	uint16_t blkTons = htons(blockNumber);
	buf[0] = htons(OPCODE_ACK);
	buf[1] = htons(blockNumber);
	printf("Sennding ack: op %hd and block# %hd\n", ntohs(opcode), ntohs(blkTons));
	
	return sendto(clientSocket, &buf, 4, 0, (struct sockaddr*)dest_adrr, sizeof(struct sockaddr_in));
}

int sendError(short errorCode, const char* errMsg, const struct sockaddr_in* source) {
	//int sizeMsg = sizeof(errMsg);
	char buf[200];
	
	short op = htons(OPCODE_ERROR);
	short err = htons(errorCode);
	((uint16_t*)buf)[0] = op;
	((uint16_t*)buf)[1] = err;
	int i = 0;
	while (errMsg[i] != '\0') {
		buf[i + 4] = errMsg[i];
		i++;
	}
	buf[i + 4] = '0';
	int len = i+4;
	printf("sending error: %s",errMsg);
	sendto(clientSocket, &buf, len, 0, (struct sockaddr*)source,
		sizeof(struct sockaddr_in));
	return 0;
}


// returns number of bytes read on success, -1 on error
//Checks if opcode and blocknumber are correct
//If so, updates the buf to contain the data (only in DATA case)
//on time out returns -3

int receive_message(int s, char buf[512], struct sockaddr_in* source) {
	printf("socket for recv= %d",s);
	socklen_t fromlen = sizeof(struct sockaddr_in);
	//char bufRecive[518];
	//setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval));
	int received = recvfrom(s, buf, SIZE+4, 0, (struct sockaddr*)source, &fromlen);
	if (DEBUG) {
		int i;
		for (i = 0; i < 512; i++)
			printf("buf[%d+4] = %c\n", i, buf[i + 4]);
	}
	/*if (received == -1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			printf("return -3\n");
			return -3;
		}
		else
			return -1;
	}*/
	return received;
}

short getOpCode(char* buf) {
	short op;
	op = buf[1];
	return op;
}


int file_error_message(const char* err_desc, struct sockaddr_in* source)
{
	if (errno == EACCES)
		sendError(2, ERRDESC_RRQ_ACCESS_VIOLATION, source);
	else if (errno == EDQUOT || errno == ENOSPC)
		sendError(3, ERRDESC_ALLOC_EXCEEDED, source);
	else
		sendError(0, err_desc, source);
	return 0;
}

int handleFirstRequest(char* bufRecive, struct sockaddr_in* source) {
	short opcode;
	char fileName[100];
	struct stat fdata;

	opcode = bufRecive[1];

	if (sscanf(bufRecive + 2, "%s", fileName) < 0) {
		perror("sscanf");
		return -1;
	}

	if (opcode == OPCODE_RRQ) {
		
		if (stat(fileName, &fdata) != 0) {
			if (errno == ENOENT || errno == ENOTDIR) {
				return sendError(1, ERRDESC_RQ_FILE_NOT_FOUND, source);
			}
		}
		if (S_ISDIR(fdata.st_mode)) {
			//error not defined
			return sendError(0, ERRDESC_RQ_FILE_IS_A_DIRECTORY, source);
		}
		file = fopen(fileName, O_RDONLY);
		if (file == NULL) {
			return file_error_message(ERRDESC_OPEN_FAILED, source);
		}
		// open a new client socket
		if (init_client()) {
			fclose(file);
			return sendError(0, ERRDESC_INTERNAL_ERROR, source);
			// send_error_message(source, ERROR_NOT_DEFINED, ERRDESC_INTERNAL_ERROR);
		}
		state = OPCODE_RRQ;
	//	blockNumber = 1;
		client = *source;

		// send_data_message(source);
	}
	else {
		// check if the file already exists
		if (stat(fileName, &fdata) == 0) {
			return sendError(6, ERRDESC_WRQ_FILE_ALREADY_EXISTS, source);
		}
		// ENOENT means there is no file, otherwise there is a problem

		if (errno != ENOENT) {
			return file_error_message(ERRDESC_STAT_FAILED, source);
		}

		// open the file
		file = fopen(fileName, "w");
		if (file == NULL) {
			return file_error_message(ERRDESC_WRQ_UNABLE_TO_CREATE_FILE, source);
		}

		// open a new client socket
		if (init_client()) {
			fclose(file);
			return sendError(0, ERRDESC_INTERNAL_ERROR, source);
		}

		state = OPCODE_WRQ;
		//blockNumber = 1;
		client = *source;

		int sent= sendAck(source);
		printf("%d\n", sent);
	}

	return 0;
}




int init_server() {
	struct sockaddr_in myaddr;

	//socklen_t addrlen = sizeof(remaddr);
	int sockfd;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("Error creating socket");
		return -1;
	}
	memset((char*)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(PORT);
	if (bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("Error binding");
		close(sockfd);
		return -1;
	}
	return sockfd;
}

static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2) {
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

int handleWriting(char* buf, const struct sockaddr_in *dest_adrr) {
	char toWrite[518];
	short op, block;
	
	op = (buf[1]);
	block = (buf[3]);
	printf("Recived op:%hd and block:%hd\n", op, block);
	int i = 0;
	while (buf[i + 4] != '\0' && i != 512) {
		
		toWrite[i] = buf[i + 4];
		i++;
	}

	toWrite[i] = '\0';
	if (block != blockNumber) {
		if (block == blockNumber - 1) {
			blockNumber--;
			sendAck(dest_adrr);
			blockNumber++;
			return -2;
		}
		else
			return -1;
	}
	//int len = strlen(toWrite);
	int write = fwrite(toWrite, 1, i, file);
	if (write != i) {
		perror("write");
		return -1;
	}
	if (DEBUG) {
		printf("Written %d\n", i);
	}
	return write;
}

int handleReading(char* buf, struct sockaddr_in* source) {
	short op, block;
	sscanf(buf, "%hd%hd", &op, &block);

	if (blockNumber != block) {
		if (block == blockNumber - 1) {
			blockNumber--;
			sendData(source);
			blockNumber++;
			return -2;
		}
	}
	else
		//sendData(file, blockNumber, serverSocket, source, sizeof(source));
		sendData(source);
	return 0;
}

/*return function that we need to use..
	return values- think about this later..*/
int handle(short op, char* buf, struct sockaddr_in* source) {
	if (state != -1 && addrcmp(source, &client)) {
		printf("Inside the if %d\n",state);
		return sendError(5, ERRDESC_UNKNOWN_TID, source);
	}
	if (op == OPCODE_RRQ || op == OPCODE_WRQ) {
		if (state != -1) {
			if (state == OPCODE_RRQ)
				return sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_READ_IN_PROGRESS, source);
			else
				return sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_WRITE_IN_PROGRESS, source);
			return -2;
		}
		if (op == OPCODE_RRQ) {
			if (state == -1)
				state = OPCODE_RRQ;
		}
		else if (op == OPCODE_WRQ) {
			if (state == -1)
				state = OPCODE_WRQ;
		}
		return handleFirstRequest(buf, source);
	}
	else if (op == OPCODE_DATA) {
		if (state == OPCODE_RRQ) {
			sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_READ_IN_PROGRESS, source);
			return -2;
		}
		else if (state == -1) {
			sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_IDLE, source);
			return -2;
		}
		else {

			int writen = handleWriting(buf, source);
			if (writen > 0) {
				sendAck(source);
				if (writen < SIZE) {//DONE
					printf("returning 1\n");
					return 1;
				}
				//blockNumber++;
			}
			return 0;
		}
		//handle writing and send ack
	}
	else if (op == OPCODE_ACK) {
		if (state == OPCODE_WRQ) {
			sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_WRITE_IN_PROGRESS, source);
			return -2;
		}
		else if (state == -1) {
			sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_IDLE, source);
			return -2;
		}
		else {
			handleReading(buf, source);
			return 0;
		}
	}
	else { printf("Why are we here?\n"); }

	return -3;
}


int main(int argc, char* argv[]) {
	//server loop:
	//int cntRead, cntWrite;
	clientSocket = 0;
	int sockfd = init_server();
	if (sockfd < 0) {
		perror("init server");
		return 0; //error- terminating program!
	}
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	blockNumber = 0;
	//struct stat statbuf;
	char buf[512];
	//char fileName[100];
	short op;
	struct sockaddr_in source;
	int recv;
	int func;
	//int errCode;
	//struct timeval time = { 3,0 };
	while (TRUE) {
		//printf("client socket1 is: %d\n", ntohs(source.sin_port));
		recv = receive_message(clientSocket == 0 ? sockfd : clientSocket, buf, &source);

		if (recv < 0) {
			printf("error here\n");
		}
		
		op = getOpCode(buf);

		func = handle(op, buf, &source);
		if (func == 1) {
			state = -1;
			//clientSocket = 0;
			close_client();
			fclose(file);
			blockNumber = 0;
			continue;
		}
		blockNumber++;
		bzero(buf, 512);
		//func= -2 or -1 error
		//else;
		// if func tell us to read:
		//sendData(file, blockNumber, sockfd, &source, sizeof(source));

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