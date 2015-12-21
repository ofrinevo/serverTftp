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

	
	if (bind(new_socket, (struct sockaddr *)&servaddr, sizeof(servaddr))<0)
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

	clientSocket= 0;
	return close(socket);
}

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

int handleFirstRequest(char* bufRecive, struct sockaddr_in* source) {
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
		if (file == -1) {
			// error
	}
		// open a new client socket
		if (init_client()) {
			close(file);
			// send_error_message(source, ERROR_NOT_DEFINED, ERRDESC_INTERNAL_ERROR);
		}
		state = OPCODE_RRQ;
		blockNumber = 1;
		client = *source;

		// send_data_message(source);
	}
	else {
		// check if the file already exists
		if (stat(fileName, &fdata) == 0){
			//TODO error
			//send_error_message(source, ERROR_FILE_ALREADY_EXISTS, ERRDESC_WRQ_FILE_ALREADY_EXISTS);
		}
		// ENOENT means there is no file, otherwise there is a problem
		if (errno != ENOENT){
			//TODO error
			//send_file_transfer_error_message(source, ERRDESC_STAT_FAILED);
		}

		// open the file
		file = open(g_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (file == -1){
			//ERROR
		}
		// open a new client socket
		if (init_client()) {
			close(file);
			// send_error_message(source, ERROR_NOT_DEFINED, ERRDESC_INTERNAL_ERROR);
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

static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2){
	return memcmp(addr1, addr2, sizeof(struct sockaddr_in));
}

int handleWriting(char* buf) {
	char toWrite[518];
	short op, block;
	sscanf(buf, "%hd%hd%s", op, block, toWrite);
	if (block != blockNumber + 1) {
		//error
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
	blockNumber = block;
	return toWrite;
}

int handleReading(char* buf) {
	short op, block;
	sscanf(buf, "%hd%hd", op, block);
	if (op == OPCODE_ACK) {
		if (blockNumber != block ) {
			//need to retransmit!
		}
		else
			sendData(file, blockNumber, serverSocket, client, sizeof(client));
	}
	else if(op==OPCODE_ERROR)
	{
		//retransmit!
	}
	else
		//unexpected opCode 
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
		return handleFirstRequest(buf,source);
	}
	else if (op == OPCODE_DATA) {
		if (state == OPCODE_RRQ || state == -1)
			return -2;
		else { 
			int writen=handleWriting(buf);
			
			sendAck(source);
			if (writen < SIZE) {//DONE
				return 1;
			}
			return 0;
		}
			//handle writing and send ack
	}
	else if (op == OPCODE_ACK ) {
		if (state == OPCODE_WRQ || state == -1)
			return -2;
		else
			//TODO..
	}
	else
		//unexpected op code- tell the son of a bitch!
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