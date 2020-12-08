#ifndef SERVER_H
#define SERVER_H

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
#define DATA_TRANSFER_CHUNK 1024 * 32
#define SERVER_PORT 8089
#define MAX_THREADS 2

#define LOG_ERROR(message) \
    printf("%s\n%s\nLine: %d\n\n", message, strerror(errno), __LINE__)

#define LOG_MSG(message) \
    printf("%s\n", message)

// Global referince to server socket
int server;

//States of a connected client

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
    CLIENT_ERROR,
    CLIENT_CLOSED
};

//Info for send_file method

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
	char args[BUFFER_CHUNK];
	unsigned int index;
};

// A structure for a linked list of client_info type
struct entry
{
	struct client_info data;
	SLIST_ENTRY(entry) entries;
};

SLIST_HEAD(slisthead, entry);

typedef int (*client_fun_type)(struct entry* client);
typedef void (*client_free_type)(void* args);

struct client_function
{
	client_fun_type work;
	client_free_type free;
};

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

void signal_handler(int sign_nr);

void delete_client(struct entry* it, struct slisthead* clients);
int insert_client(struct slisthead* clients, struct entry client);

void delete_command(struct entry* client);

int send_message(int socket, const char* message, int len);
int recv_message(int socket, char* buffer, int expected_len);

int socket_read(struct entry* client);
int socket_write(struct entry* client);

void exit_thread(struct slisthead* clients, int index);

void dispatch_client(int client_socket);
void* handle_client(void* args);

int send_file(struct entry* client);
void send_file_free(void* args);

int show_files(struct entry* client);

int execute_command(struct entry* client);

void handle_result(struct entry* client, struct slisthead* clients, int index, enum client_result result);

int init_thread(struct worker_type* workers, int index);
int setup_server();

int run();

#endif
