#ifndef _SERVER_H
#define _SERVER_H

#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <dirent.h>

#define BACK_LOG 10
#define BUFFER_CHUNK 1024
#define SERVER_PORT 8089
#define MAX_THREADS 2

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

#define LOG_MSG(message) \
    printf("%s\n", message);

// Global referince to server socket
int server;

//States of a connected client

enum states
{
	login,
	connected,
	in_transfer
};

// A structure shere we save all the informations about the clients
struct client_info
{
	int socket;
	enum states state;
	void* args;
};

//all the avaible

enum commands_list
{
	get_files_list,
	get_file
};
//command structure

struct command
{
	char args[BUFFER_CHUNK];
	int index;
};

// A structure for a linked list of client_info type
struct entry
{
	struct client_info data;
	SLIST_ENTRY(entry) entries;
};

SLIST_HEAD(slisthead, entry);

//Mutex for thread syncronization
pthread_mutex_t mutex;// = PTHREAD_MUTEX_INITIALIZER;

//Global array where we put how many clients every thread have
int clients_connected[MAX_THREADS];

//Structure for passing arguments to the thread
struct thread_arg
{
	int socket;
	int index;
};

//Structure where we save the handle to every thead and communications socket from socketpair()
struct worker_type
{
	pthread_t thread_handle;
	int socket;
};

//Array for saving data about all the workers
struct worker_type workers[MAX_THREADS];

int parse_command(const struct command* cmd);

void signal_handler(int sign_nr);

void delete_socket(struct slisthead* clients, int socket);
int insert_client(struct slisthead* clients, struct entry client);
struct entry* get_element(struct slisthead* clients, int socket);

int send_message(int socket, const char* message, int len);
int recv_message(int socket, char* buffer, int expected_len);

int socket_read(int client_socket, struct slisthead* clients, fd_set* master_fd, fd_set* write_fd, int index);
int socket_write(struct slisthead* clients, int client_socket, fd_set* write_fd);

void exit_thread(struct slisthead* clients, int index);

void dispatch_client(struct worker_type* workers, int client_socket);
void* handle_client(void* args);

int init_thread(struct worker_type* workers);
int setup_server();

int run();

#endif