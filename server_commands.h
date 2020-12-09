#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H
#include "types.h"
#include <dirent.h>

#define DATA_TRANSFER_CHUNK 1024 * 32

typedef enum client_result(*client_fun_type)(struct entry* client);
typedef void (*client_free_type)(void* args);

int send_message(int socket, const char* message, int len);
int recv_message(int socket, char* buffer, int expected_len);

struct client_function
{
	client_fun_type work;
	client_free_type free;
};

struct transfer_info
{
	int file_descriptor;
	int state;
};

struct file_info
{
	off_t file_size;
	int file_type;
};

enum client_result send_file(struct entry* client);
void send_file_free(void* args);

#endif