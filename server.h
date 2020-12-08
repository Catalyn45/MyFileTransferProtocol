#ifndef SERVER_H
#define SERVER_H

#include "types.h"
#include "server_commands.h"
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>

// Global referince to server socket

int server;

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

void signal_handler(int sign_nr);

void delete_client(struct entry* it, struct slisthead* clients);
int insert_client(struct slisthead* clients, struct entry client);

void delete_command(struct entry* client);

int send_message(int socket, const char* message, int len);
int recv_message(int socket, char* buffer, int expected_len);

enum client_result socket_read(struct entry* client);
enum client_result socket_write(struct entry* client);

void exit_thread(struct slisthead* clients, int index);

void dispatch_client(int client_socket);
void* handle_client(void* args);

enum client_result execute_command(struct entry* client);

void handle_result(struct entry* client, struct slisthead* clients, int index, enum client_result result);

int init_thread(struct worker_type* workers, int index);
int setup_server();

int run();

#endif
