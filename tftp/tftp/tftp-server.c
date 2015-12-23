/*
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
#define PORT 57981
#define TRUE 1
#define FALSE 0
#define DEBUG 1
#define NAX_RETRANS 4
short state = -1;
#define OP_CODE = sizeof(short);
struct timeval timeout;
struct sockaddr_in client;
int retransmitions = 0;

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

	if (fseek(file, (blockNumber - 1)*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE + 4];
	bzero(buf, SIZE + 4);

	short opcode = htons(OPCODE_DATA);
	short blkTons = htons(blockNumber);
	((uint16_t*)buf)[0] = opcode;
	((uint16_t*)buf)[1] = blkTons;


	int sizeRead = fread(buf + 4, 1, SIZE, file);

	if (ferror(file)) {
		perror("fread");
		return -1;
	}
	sendto(clientSocket, &buf, sizeRead + 4, 0, (struct sockaddr*)dest_adrr,
		sizeof(struct sockaddr_in));
	return sizeRead;
}

//returns 0 on success, -1 else
int sendAck(const struct sockaddr_in *dest_adrr) {
	uint16_t buf[2];
	buf[0] = htons(OPCODE_ACK);
	buf[1] = htons(blockNumber);
	return sendto(clientSocket, &buf, 4, 0, (struct sockaddr*)dest_adrr, sizeof(struct sockaddr_in));
}

int sendError(short errorCode, const char* errMsg, const struct sockaddr_in* source) {
	//int sizeMsg = sizeof(errMsg);
	char buf[200];
	bzero(buf, 200);
	((uint16_t*)buf)[0] = htons(OPCODE_ERROR);
	((uint16_t*)buf)[1] = htons(errorCode);
	int i = 0;
	while (errMsg[i] != '\0') {
		buf[i + 4] = errMsg[i];
		i++;
	}
	buf[i + 4] = '\0';
	buf[i + 5] = '0';
	buf[i + 6] = EOF;
	sendto(clientSocket==0 ? serverSocket : clientSocket, &buf, 200, 0, (struct sockaddr*)source,
		sizeof(struct sockaddr_in));
	return 0;
}


// returns number of bytes read on success, -1 on error
//Checks if opcode and blocknumber are correct
//If so, updates the buf to contain the data (only in DATA case)
//on time out returns -3

int receive_message(int s, char buf[512], struct sockaddr_in* source) {

	socklen_t fromlen = sizeof(struct sockaddr_in);
	int received = recvfrom(s, buf, SIZE + 4, 0, (struct sockaddr*)source, &fromlen);
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
				sendError(1, ERRDESC_RQ_FILE_NOT_FOUND, source);
				return -4;
			}
		}
		if (S_ISDIR(fdata.st_mode)) {
			//error not defined
			sendError(0, ERRDESC_RQ_FILE_IS_A_DIRECTORY, source);
			return -4;
		}
		file = fopen(fileName, "r");
		if (file == NULL) {
			file_error_message(ERRDESC_OPEN_FAILED, source);
			return -4;
		}
		// open a new client socket
		if (init_client()) {
			fclose(file);
			sendError(0, ERRDESC_INTERNAL_ERROR, source);
			return -4;

		}
		state = OPCODE_RRQ;
		blockNumber = 1;
		client = *source;

		sendData(source);
	}
	else {
		// check if the file already exists
		if (stat(fileName, &fdata) == 0) {
			sendError(6, ERRDESC_WRQ_FILE_ALREADY_EXISTS, source);
			return -4;
		}
		// ENOENT means there is no file, otherwise there is a problem

		if (errno != ENOENT) {
			file_error_message(ERRDESC_STAT_FAILED, source);
			return -4;
		}

		// open the file
		file = fopen(fileName, "w");
		if (file == NULL) {
			file_error_message(ERRDESC_WRQ_UNABLE_TO_CREATE_FILE, source);
			return -4;
		}

		// open a new client socket
		if (init_client()) {
			fclose(file);
			sendError(0, ERRDESC_INTERNAL_ERROR, source);
			return -4;
		}

		state = OPCODE_WRQ;
		//blockNumber = 1;
		client = *source;

		sendAck(source);

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
	serverSocket = sockfd;
	return sockfd;
}

static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2) {
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

int handleWriting(char* buf, const struct sockaddr_in *dest_adrr) {
	char toWrite[518];
	short  block;
	block = ntohs(((uint16_t*)buf)[1]);

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

	return write;
}

int handleReading(char* buf, struct sockaddr_in* source) {
	uint16_t  block;
	block = ntohs(((uint16_t*)buf)[1]);
	if (blockNumber != block) {
		if (block == blockNumber - 1) {
			sendData(source);
			return -2;
		}
	}
	else {
		blockNumber++;
		int sent = sendData(source);
		if (sent < 512)
			return 1;
	}
	return 0;
}

*/
/*return function that we need to use..
	return values- think about this later..*/
/*
int handle(short op, char* buf, struct sockaddr_in* source) {
	if (state != -1 && addrcmp(source, &client)) {

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

			return handleReading(buf, source);
		}
	}
	else { return -3; }

	return -3;
}


int main(int argc, char* argv[]) {
	clientSocket = 0;
	int sockfd = init_server();
	if (sockfd < 0) {
		perror("init server");
		return 0; //error- terminating program!
	}
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	blockNumber = 0;
	char buf[512];
	short op;
	struct sockaddr_in source;
	int recv;
	int func;
	while (TRUE) {

		recv = receive_message(clientSocket == 0 ? sockfd : clientSocket, buf, &source);

		if (recv < 0) {
			printf("error here\n");
		}

		op = getOpCode(buf);

		func = handle(op, buf, &source);
		if (func == 1) {
			state = -1;
			close_client();
			fclose(file);
			blockNumber = 0;
			continue;
		}
		if (func == -4) {
			state = -1;
			blockNumber = 0;
			continue;
		}
		if (state == OPCODE_WRQ)
			blockNumber++;
		bzero(buf, SIZE + 4);
	}
	return 0;
}

//
///*operation: message to retransmit:
//		1=ACK	2= DATA*/
//int retransmit(int operation, struct sockaddr_in *dest_adrr)
//{
//	int status;
//	// if we didn't retransmit MAX_RETRANSMISSIONS times, transmit again!
//	// maybe the message got lost. otherwise - disconnect
//	if (retransmitions >= NAX_RETRANS)
//	{
//		// close the file, disconnect from client
//		status = close(file);
//		
//		state = -1;
//		return close_client() || status;
//	}
//	else
//	{
//		if (operation == 1)
//			sendAck(dest_adrr);
//		else if (operation == 2)
//			sendData(dest_adrr);
//		retransmitions++;
//		return status;
//	}
//}
*/

#include "tftp-server.h"

#define SIZE 512
#define PORT 57981
#define TRUE 1
#define FALSE 0
#define DEBUG 1
#define NAX_RETRANS 4

#define OP_CODE = sizeof(short);

struct timeval timeout;
struct sockaddr_in client;
int retransmitions = 0;
short state = -1;

int last_op = -1;
int done_write = FALSE;

int blockNumber;
int clientSocket;
int serverSocket;
FILE* file = NULL;


/*operation: message to retransmit:
1=ACK	2= DATA
return 1 if retranssimted max_transsmitions
*/
int retransmit(int operation, const struct sockaddr_in *dest_adrr)
{
	// if we didn't retransmit MAX_RETRANSMISSIONS times, transmit again!
	// maybe the message got lost. otherwise - disconnect
	if (retransmitions >= NAX_RETRANS)
	{
		return 1;// SIGNAL the server to close the client and seek for other clients
	}
	else
	{
		if (operation == 1)
			sendAck(dest_adrr);
		else if (operation == 2)
			sendData(dest_adrr);
		retransmitions++;
		return -2; //arbitrary return value.. can be something else.. but not 1 or -4
	}
}


//return sockfd, or -1 on error;
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
	serverSocket = sockfd;
	return sockfd;
}

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

int sendError(short errorCode, const char* errMsg, const struct sockaddr_in* source) {
	//int sizeMsg = sizeof(errMsg);
	char buf[200];
	bzero(buf, 200);
	((uint16_t*)buf)[0] = htons(OPCODE_ERROR);
	((uint16_t*)buf)[1] = htons(errorCode);
	int i = 0;
	while (errMsg[i] != '\0') {
		buf[i + 4] = errMsg[i];
		i++;
	}
	buf[i + 4] = '\0';
	buf[i + 5] = '0';
	buf[i + 6] = EOF;
	sendto(clientSocket == 0 ? serverSocket : clientSocket, &buf, 200, 0, (struct sockaddr*)source,
		sizeof(struct sockaddr_in));
	return 0;
}

int file_error_message(const char* err_desc, const struct sockaddr_in* source)
{
	if (errno == EACCES)
		sendError(2, ACCESS_VIOLATION, source);
	else if (errno == EDQUOT || errno == ENOSPC)
		sendError(3, DF_OR_AE, source);
	else
		sendError(0, err_desc, source);
	return 0;
}


//return num of bytes read from the file on success, -1 else
int sendData(const struct sockaddr_in *dest_adrr) {
	if (fseek(file, (blockNumber - 1)*SIZE, SEEK_SET)) {
		perror("fseek");
		return -1;
	}
	char buf[SIZE + 4];
	bzero(buf, SIZE + 4);

	short opcode = htons(OPCODE_DATA);
	short blkTons = htons(blockNumber);
	((uint16_t*)buf)[0] = opcode;
	((uint16_t*)buf)[1] = blkTons;


	int sizeRead = fread(buf + 4, 1, SIZE, file);

	if (ferror(file)) {
		file_error_message(READ_FAILED, dest_adrr);
		perror("fread");
		return -1;
	}
	retransmitions = 0;
	sendto(clientSocket, &buf, sizeRead + 4, 0, (struct sockaddr*)dest_adrr,
		sizeof(struct sockaddr_in));
	return sizeRead;
}

//returns 0 on success, -1 else
int sendAck(const struct sockaddr_in *dest_adrr) {
	uint16_t buf[2];
	buf[0] = htons(OPCODE_ACK);
	buf[1] = htons(blockNumber);
	retransmitions = 0;
	return sendto(clientSocket, &buf, 4, 0, (struct sockaddr*)dest_adrr, sizeof(struct sockaddr_in));
}


// returns number of bytes read on success, -1 on error
//Checks if opcode and blocknumber are correct
//If so, updates the buf to contain the data (only in DATA case)
//on time out returns -3

int receive_message(int s, char buf[512], const struct sockaddr_in* source) {

	socklen_t fromlen = sizeof(struct sockaddr_in);
	int received = recvfrom(s, buf, SIZE + 4, 0, (struct sockaddr*)source, &fromlen);
	return received;
}

short getOpCode(char* buf) {
	short op;
	op = buf[1];
	return op;
}

int handleFirstRequest(char* bufRecive, const struct sockaddr_in* source) {
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
				sendError(1, NO_FILE, source);
				return -4;
			}
		}
		if (S_ISDIR(fdata.st_mode)) {
			//error not defined
			sendError(0, FILE_IS_DIRCTORY, source);
			return -4;
		}
		file = fopen(fileName, "r");
		if (file == NULL) {
			file_error_message(OPEN_FAILED, source);
			return -4;
		}
		// open a new client socket
		if (init_client()) {
			fclose(file);
			sendError(0, SERVER_ERROR, source);
			return -4;

		}
		state = OPCODE_RRQ;
		blockNumber = 1;
		client = *source;

		sendData(source);
		last_op = 2; //DATA
	}
	else {
		// check if the file already exists
		if (stat(fileName, &fdata) == 0) {
			sendError(6, FILE_EXIST, source);
			return -4;
		}
		// ENOENT means there is no file, otherwise there is a problem

		if (errno != ENOENT) {
			file_error_message(STAT_FAILED, source);
			return -4;
		}

		// open the file
		file = fopen(fileName, "w");
		if (file == NULL) {
			file_error_message(CANT_CREATE_FILE, source);
			return -4;
		}

		// open a new client socket
		if (init_client()) {
			fclose(file);
			sendError(0, SERVER_ERROR, source);
			return -4;
		}

		state = OPCODE_WRQ;
		//blockNumber = 1;
		done_write = FALSE;
		client = *source;

		sendAck(source);
		last_op = 1; //ACK
	}

	return 0;
}


static int addrcmp(const struct sockaddr_in* addr1,const struct sockaddr_in* addr2) {
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

int handleWriting(char* buf, const struct sockaddr_in *dest_adrr) {
	char toWrite[518];
	short  block;
	block = ntohs(((uint16_t*)buf)[1]);

	int i = 0;
	while (buf[i + 4] != '\0' && i != 512) {

		toWrite[i] = buf[i + 4];
		i++;
	}

	toWrite[i] = '\0';

	if (block != blockNumber+1) {
		if (block == blockNumber) {
			return retransmit(1, dest_adrr);
			/*blockNumber--;
			sendAck(dest_adrr);
			blockNumber++;*/
			//return -2;
		}
		else
			return -1;
	}
	//int len = strlen(toWrite);
	int write = fwrite(toWrite, 1, i, file);
	if (write != i) {
		perror("write");
		file_error_message(WRITE_FAILED, dest_adrr);
		return -1;
	}
	blockNumber = block;

	return write;
}

int handleReading(char* buf, const struct sockaddr_in* source) {
	uint16_t  block;
	block = ntohs(((uint16_t*)buf)[1]);
	if (blockNumber != block) {
		if (block == blockNumber - 1) {
			return retransmit(2, source);
			//sendData(source);
			//return -2;
		}
	}
	else {
		blockNumber++;
		int sent = sendData(source);
		last_op = 2;//DATA
		if (sent < 512)
			return 1;
	}
	return 0;
}

/*return function that we need to use..
	return values- think about this later..*/
int handle(short op, char* buf, const struct sockaddr_in* source) {
	if (state != -1 && addrcmp(source, &client)) {

		return sendError(5, UNKNOWN_TID, source);
	}
	if (op == OPCODE_RRQ || op == OPCODE_WRQ) {
		if (state != -1) {
			if (state == OPCODE_RRQ)
				return sendError(4, UNEXPECTED_OPCODE_RRQ, source);
			else
				return sendError(4, UNEXPECTED_OPCODE_WRQ, source);
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
			sendError(4, UNEXPECTED_OPCODE_RRQ, source);
			return -2;
		}
		else if (state == -1) {
			sendError(4, UNEXPECTED_OPCODE_IDLE, source);
			return -2;
		}
		else {

			int writen = handleWriting(buf, source);
			if (writen > 0) {
				sendAck(source);
				last_op = 1; //ACK
				if (writen < SIZE) {//DONE
					
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
			sendError(4, UNEXPECTED_OPCODE_WRQ, source);
			return -2;
		}
		else if (state == -1) {
			sendError(4, UNEXPECTED_OPCODE_IDLE, source);
			return -2;
		}
		else {

			return handleReading(buf, source);
		}
	}
	else { return -3; }

	return -3;
}


int main(int argc, char* argv[]) {
	clientSocket = 0;
	int sockfd = init_server();
	if (sockfd < 0) {
		perror("init server");
		return 0; //error- terminating program!
	}
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	blockNumber = 0;
	char buf[512];
	short op;
	const struct sockaddr_in source;
	int recv;
	int func;
	int status;
	while (TRUE) {

		recv = receive_message(clientSocket == 0 ? sockfd : clientSocket, buf, &source);

		if (recv < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (last_op != 0) {
					status = retransmit(last_op, &source);
					if (status == 1)
						closeConnection();
				}
				else {
					printf("Something went wrong!\n");
				}
			}
			else
				printf("UnKnown error\n");
		}

		op = getOpCode(buf);

		func = handle(op, buf, &source);
		if (func == 1) {
			closeConnection();
			continue;
		}
		if (func == -4) {
			state = -1;
			blockNumber = 0;
			continue;
		}

		//if (state == OPCODE_WRQ)
			//blockNumber++;
		bzero(buf, SIZE + 4);
	}
	return 0;
}

void closeConnection() {
	state = -1;
	close_client();
	fclose(file);
	blockNumber = 0;
	last_op = 0;
}



