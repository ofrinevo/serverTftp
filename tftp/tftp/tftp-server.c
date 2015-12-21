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
#define PORT 69
#define TRUE 1
#define FALSE 0
#define DEBUG 1
uint16_t state = -1;
#define OP_CODE= sizeof(uint16_t);
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
	uint16_t blockNumber;
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
	return 1;
}


int close_client() {
	int socket = clientSocket;

	if (socket == 0)
		return 1;

	clientSocket = 0;
	return close(socket);
}

//return num of bytes read from the file on success, -1 else
int sendData(const struct sockaddr *dest_adrr) {
	if (fseek(file, blockNumber*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE + 4];
	uint16_t opcode = htons(OPCODE_DATA);
	uint16_t blkTons = htons(blockNumber);
	if (sprintf(buf, "%hd%hd", opcode, blkTons)) {
		perror("sprintf");
		return -1;
	}

	int sizeRead = fread(buf, SIZE, 1, file);
	if (ferror(file)) {
		perror("fread");
		return -1;
	}
	sendto(clientSocket, buf, SIZE + 4, 0, dest_adrr, sizeof(dest_adrr));
	return sizeRead;
}

//returns 0 on success, -1 else
int sendAck(const struct sockaddr *dest_adrr) {
	char buf[4];
	uint16_t opcode = htons(OPCODE_DATA);
	uint16_t blkTons = htons(blockNumber);
	if (sprintf(buf, "%hd%hd", opcode, blkTons) < 0) {
		perror("sprintf");
		return -1;
	}
	sendto(clientSocket, buf, 4, 0, dest_adrr, sizeof(dest_adrr));
	return 0;
}

int sendError(uint16_t errorCode, char* errMsg, struct sockaddr_in* source) {
	int sizeMsg = sizeof(errMsg);
	char buf[200];
	int len = 2 * sizeof(short) + sizeMsg + sizeof(short);
	short op = OPCODE_ERROR;
	if (sprintf(buf, "%hd%hd%s0", op,errorCode, errMsg) < 0) {
		perror("sprintf");
		return -1;
	}
	sendto(clientSocket, buf, len, 0, source, sizeof(source));
	return 0;
}


// returns number of bytes read on success, -1 on error
//Checks if opcode and blocknumber are correct
//If so, updates the buf to contain the data (only in DATA case)
//on time out returns -3

int receive_message(int s, char* buf, struct sockaddr_in* source, uint16_t opcode, uint16_t blockNumber) {
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
	uint16_t currBlockNum, currOpCode;
	sscanf(bufRecive, "%hd%hd%s", currOpCode, currBlockNum, buf);
	if (currBlockNum != blockNumber)
		return -2;
	if (currOpCode != opcode)
		return -2;
	return received;
}

uint16_t getOpCode(char* buf) {
	uint16_t op;
	if (sscanf(buf, "%hd", op) < 0) {
		perror("sscanf");
		return -1;;
	}
	return op;
}


int file_error_message(const char* err_desc, struct sockaddr_in* source)
{
	if (errno == EACCES)
		sendError(2, ERRDESC_RRQ_ACCESS_VIOLATION,source);
	else if (errno == EDQUOT || errno == ENOSPC)
		sendError(3, ERRDESC_ALLOC_EXCEEDED,source);
	else
		sendError(0, err_desc,source);
}

int handleFirstRequest(char* bufRecive, struct sockaddr_in* source) {
	uint16_t opcode;
	char* fileName;
	struct stat fdata;
	if (sscanf(bufRecive, "%hd0%s", opcode, fileName) != 2) {
		perror("sscanf");
		return -1;
	}
	if (opcode == OPCODE_RRQ) {
		if (stat(fileName, &fdata) != 0) {
			if (errno == ENOENT || errno == ENOTDIR) {
				sendError(1, ERRDESC_RQ_FILE_NOT_FOUND,source);
			}
		}
		if (S_ISDIR(fdata.st_mode)) {
			//error not defined
			sendError(0, ERRDESC_RQ_FILE_IS_A_DIRECTORY,source);
		}
		file = fopen(fileName, O_RDONLY);
		if (file == -1) {
			file_error_message(ERRDESC_OPEN_FAILED,source);
	}
		// open a new client socket
		if (init_client()) {
			close(file);
			sendError(0, ERRDESC_INTERNAL_ERROR, source);
			// send_error_message(source, ERROR_NOT_DEFINED, ERRDESC_INTERNAL_ERROR);
		}
		state = OPCODE_RRQ;
		blockNumber = 1;
		client = *source;

		// send_data_message(source);
	}
	else {
		// check if the file already exists
		if (stat(fileName, &fdata) == 0) {
			sendError(6, ERRDESC_WRQ_FILE_ALREADY_EXISTS,source);
		}
		// ENOENT means there is no file, otherwise there is a problem
		if (errno != ENOENT) {
			file_error_message(ERRDESC_STAT_FAILED,source);
		}

		// open the file
		file = open(g_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (file == -1) {
			file_error_message(ERRDESC_WRQ_UNABLE_TO_CREATE_FILE,source);
		}
		// open a new client socket
		if (init_client()) {
			close(file);
			sendError(0, ERRDESC_INTERNAL_ERROR,source);
		}

		state = OPCODE_WRQ;
		blockNumber = 0;
		client = *source;

		// send_ack_message(source);
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

static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2) {
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

int handleWriting(char* buf, const struct sockaddr *dest_adrr) {
	char toWrite[518];
	uint16_t op, block;
	sscanf(buf, "%hd%hd%s", op, block, toWrite);
	if (block != blockNumber) {
		if (block == blockNumber - 1) {
			blockNumber--;
			sendAck(dest_adrr);
			blockNumber++;
			return -2;
		}
	}
	int len = strlen(toWrite);
	int write = fwrite(toWrite, 1, len, file);
	if (write != len) {
		perror("write");
		return -1;
	}
	if (DEBUG) {
		printf("Written %d\n", len);
}
	return len;
}

int handleReading(char* buf, const struct sockaddr *dest_adrr) {
	uint16_t op, block;
	sscanf(buf, "%hd%hd", op, block);
	
	if (blockNumber != block ) {
		if (block == blockNumber - 1) {
			blockNumber--;
			sendData(dest_adrr);
			blockNumber++;
			return -2;
		}
		//need to retransmit!
	}
	else
		sendData(dest_adrr);
	
}

/*return function that we need to use..
	return values- think about this later..*/
int handle(uint16_t op, char* buf, struct sockaddr_in* source) {
	if (clientSocket != 0 && addrcmp(source, &clientSocket)) {
		sendError(5, ERRDESC_UNKNOWN_TID, source);
	}
	if (op == OPCODE_RRQ || op == OPCODE_WRQ) {
		if (state != -1) {
			if (state == OPCODE_RRQ)
				sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_READ_IN_PROGRESS,source);
			else
				sendError(4, ERRDESC_UNEXPECTED_OPCODE_DURING_WRITE_IN_PROGRESS, source);
			return -2;
		}
		if (op == OPCODE_RRQ) {
			if (state == -1)
				state = OPCODE_RRQ;
		}
		else if (op == OPCODE_WRQ) {
			if (state == -1)
				state = OPCODE_RRQ;
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
					return 1;
				}
				blockNumber++;
			}
			return 0;
		}

	}
	else if (op == OPCODE_ACK ) {
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
			
		}
			//TODO..
	}
	else {/*can it be?*/}
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
	blockNumber = 0;
	struct stat statbuf;
	char buf[512];
	char fileName[100];
	uint16_t op;
	struct sockaddr_in source;
	int recv;
	int func;
	int errCode;
	struct timeval time = { 3,0 };

	while (TRUE) {
		recv = receive_message(clientSocket == 0 ? sockfd : clientSocket, buf, &source, );
		if (recv < 0) {
			//handle..
		}
		op = getOpCode(buf);

		func = handle(op, buf, &source);
		if (func == 1)
			clientSocket = 0;
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