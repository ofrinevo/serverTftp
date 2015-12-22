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


/////////////////////////ERROR-MSG-DEFINES////////////////////////////////////////////
#define UNKNOWN_TID "Unknown transfer ID."
#define UNEXPECTED_OPCODE_IDLE "Received an unexpected opcode while expecting RRQ or WRQ"
#define UNEXPECTED_OPCODE_RRQ "Received an unexpected opcode while expecting ACK"
#define UNEXPECTED_OPCODE_WRQ "Received an unexpected opcode while expecting DATA"
#define NO_FILE "File not found."
#define DF_OR_AE "Disk full or allocation exceeded."
#define FILE_IS_DIRCTORY "Requested file is a directory"
#define FILE_EXIST "File to be written already exists"
#define CANT_CREATE_FILE "Unable to create the new file"
#define ACCESS_VIOLATION "Unable to access the requested file"
#define OPEN_FAILED "Unable to open requested file"
#define STAT_FAILED "Unable to stat requested file"
#define READ_FAILED	"Unable to read from requested file"
#define WRITE_FAILED "Unable to write to the requested file"
#define SERVER_ERROR "Internal server error."
/////////////////////////////////////////////////////////////////////



/////////////////////////////OPCODE_SUPPORTED_DEFINES//////////////////////////
#define OPCODE_RRQ 1
#define OPCODE_WRQ 2
#define OPCODE_DATA 3
#define OPCODE_ACK 4
#define OPCODE_ERROR 5
/////////////////////////////////////////////////////////////


typedef struct readSize {
	short blockNumber;
	int sizeRead;
} readSize;

int retransmit(int operation,const struct sockaddr_in *dest_adrr);
int init_client();
int close_client();
int sendError(short errorCode, const char* errMsg, const struct sockaddr_in* source);
int file_error_message(const char* err_desc, const struct sockaddr_in* source);
int sendData(const struct sockaddr_in *dest_adrr);
int sendAck(const struct sockaddr_in *dest_adrr);
int receive_message(int s, char buf[512], struct sockaddr_in* source);
short getOpCode(char* buf);
int handleFirstRequest(char* bufRecive, struct sockaddr_in* source);
int init_server();
static int addrcmp(struct sockaddr_in* addr1, struct sockaddr_in* addr2);
int handleWriting(char* buf, const struct sockaddr_in *dest_adrr);
int handleReading(char* buf, struct sockaddr_in* source);
int handle(short op, char* buf, struct sockaddr_in* source);
int main(int argc, char* argv[]);
