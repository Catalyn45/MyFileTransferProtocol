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
#include <pthread.h>
#include <sys/mman.h>

#define BACK_LOG 10
#define MAX_ARGS_LEN 1024
#define SERVER_PORT 8089
#define MAX_THREADS 2
#define MAX_PATH 256
#define MAX_ERROR_LENGTH 512

#define ACCOUNTS_FILE "accounts.conf"
#define CLIENTS_DIRECTORY "clients_home"

#define LOG_ERROR(message) \
    printf("%s\n%s\nLine: %d\n\n", message, strerror(errno), __LINE__)

#define LOG_MSG(message) \
    printf("%s\n", message)

#define LENGTH_OF(x) sizeof(x) / sizeof(*(x))

struct mapped_file
{
	char* buffer;
	off_t length;
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

enum client_events
{
	EVENT_READ,
	EVENT_WRITE,
	EVENT_TIMEOUT
};

enum command_state
{
	COMMAND_NONE,
	COMMAND_READY,
	COMMAND_ERROR
};

enum security
{
	SECURITY_ALL,
	SECURITY_CONNECTED,
	SECURITY_ADMIN
};

struct command
{
	enum command_state cmd_state;

	int ok_finished;
	int ok;
	int ok_size;

	unsigned int index;
};

struct error_info
{
	const int err_num;
	int err_length;
};

struct error_type
{
	int state;
	char* buffer;
	unsigned int error_size;
	unsigned int error_length;
};

// A structure shere we save all the informations about the clients
struct client_info
{
	int socket;
	enum security client_security;
	fd_set* read;
	fd_set* write;
	struct command current_command;
	char working_directory[MAX_PATH];
	struct error_type error;
	void* args;
};

// A structure for a linked list of client_info type
struct entry
{
	struct client_info data;
	SLIST_ENTRY(entry) entries;
};

typedef enum client_result(*client_fun_type)(struct entry* client, enum client_events event);
typedef void (*client_free_type)(struct entry* client);
typedef int (*client_new_type)(struct entry* client);
int set_error(struct entry* client, const char* message);
struct mapped_file get_accounts(const char* filename);

struct client_function
{
	enum security function_security;
	client_new_type new;
	client_fun_type work;
	client_free_type free;
};

#endif