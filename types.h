#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/queue.h>

#define BACK_LOG 10
#define MAX_ARGS_LEN 1024
#define SERVER_PORT 8089
#define MAX_THREADS 2

#define LOG_ERROR(message) \
    printf("%s\n%s\nLine: %d\n\n", message, strerror(errno), __LINE__)

#define LOG_MSG(message) \
    printf("%s\n", message)

#define LENGTH_OF(x) sizeof(x) / sizeof(*(x))

enum states
{
	login,
	connected
};

enum client_result
{
    CLIENT_SUCCESS,
    CLIENT_WANT_READ,
    CLIENT_WANT_WRITE,
    CLIENT_AGAIN,
    CLIENT_ERROR,
    CLIENT_CLOSED
};

// A structure shere we save all the informations about the clients
struct client_info
{
	int socket;
	enum states state;
	fd_set* read;
	fd_set* write;
	struct command* current_command;
	void* args;
};

struct command
{
	char args[MAX_ARGS_LEN];
	unsigned int index;
};

// A structure for a linked list of client_info type
struct entry
{
	struct client_info data;
	SLIST_ENTRY(entry) entries;
};

#endif