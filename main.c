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

#define BACK_LOG 10
#define BUFFER_CHUNK 1024
#define SERVER_PORT 8089
#define MAX_THREADS 20

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

int server;

struct client_info
{
	int socket;
	char name[20];
	int command;
	int state;
};

struct entry
{
	struct client_info data;
	SLIST_ENTRY(entry) entries;
};

SLIST_HEAD(slisthead, entry);

void delete_socket(struct slisthead* clients, int socket)
{
	struct entry* it;

	SLIST_FOREACH(it, clients, entries)
	{
		if(it->data.socket == socket)
		{
			SLIST_REMOVE(clients, it, entry, entries);
			close(it->data.socket);
			free(it);
			return;
		}
	}
}

void insert_client(struct slisthead* clients, struct entry client)
{
	struct entry* client_info = malloc(sizeof(struct entry));
	memcpy(client_info, &client, sizeof(struct entry));
	SLIST_INSERT_HEAD(clients, client_info, entries);
}

struct entry* get_element(struct slisthead* clients, int socket)
{
	struct entry* it;

	SLIST_FOREACH(it, clients, entries)
	{
		if(it->data.socket == socket)
		{
			return it;
		}
	}

	return NULL;
}

void socket_read(int client_socket, struct slisthead* clients, fd_set* master_fd, fd_set* write_fd)
{
	char buffer[BUFFER_CHUNK];
	int len = recv(client_socket, buffer, BUFFER_CHUNK - 1, 0);

	if(len <= 0)
	{
		FD_CLR(client_socket, master_fd);
		delete_socket(clients, client_socket);
		printf("Client disconnected\n");
	}
	else
	{
		buffer[len] = '\0';
		if(strstr(buffer, "vreau"))
		{
			FD_SET(client_socket, write_fd);
		}
		else
			printf("%s\n", buffer);
	}
}

void socket_write(int client_socket, fd_set* write_fd)
{
	const char* msg = "Ti-am trimis mesaj inapoi";
	send(client_socket, msg, strlen(msg), 0);
	FD_CLR(client_socket, write_fd);
}

void* handle_client(void* args)
{
    int main_thread_socket = *((int *)args);
    free(args);

    struct slisthead clients;
    SLIST_INIT(&clients);
    fd_set master_fd, read_fd, write_fd;
 
    FD_ZERO(&master_fd);
    FD_ZERO(&write_fd);

    FD_SET(main_thread_socket, &master_fd);

    while(1)
    {
    	read_fd = master_fd;

    	select(FD_SETSIZE, &read_fd, &write_fd, NULL, NULL);

    	printf("trec prin while\n");

    	for(int i = 0; i < FD_SETSIZE; i++)
    	{
    		if(FD_ISSET(i, &read_fd))
    		{
    			if(i == main_thread_socket)
    			{
    				printf("Client connected\n");
    				fflush(stdout);

    				int socket_to_add = 0;
    				recv(i, &socket_to_add, sizeof(int), 0);

    				FD_SET(socket_to_add, &master_fd);

    				struct entry client;
    				memset(&client, 0, sizeof(client));
    				client.data.socket = i;
    				
    				insert_client(&clients, client);
    			}
    			else
    			{
    				socket_read(i, &clients, &master_fd, &write_fd);
    			}
    		}
    		else if(FD_ISSET(i, &write_fd))
    		{
    			printf("trimit un mesaaaaj\n");
    			fflush(stdout);
    			socket_write(i, &write_fd);
    		}
    	}

    }
    return NULL;
}

int avaible_thread;

struct worker_type
{
	pthread_t thread_handle;
	int socket;
};

void dispatch_client(struct worker_type* workers, int client_socket)
{
	avaible_thread++;

	if(avaible_thread == MAX_THREADS)
		avaible_thread = 0;

	send(workers[avaible_thread].socket, &client_socket, sizeof(int), 0);
}

int init_poll(struct worker_type* workers)
{
    int i;
    for(i = 0; i < MAX_THREADS; i++)
    {
    	int sockp[2];

    	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)
    		break;

    	workers[i].socket = sockp[0];
    	int* client_socket = malloc(sizeof(int));

    	*client_socket = sockp[1];

        if(pthread_create(&workers[i].thread_handle, NULL, handle_client, client_socket) == -1)
            break;
    }

    if(i != MAX_THREADS)
    {
        for(int j = 0; j < i; j++)
        {
            pthread_exit(&workers[j].thread_handle);
            close(workers[j].socket);
        }

        return -1;
    }

    return 0;
}

void signal_handler(int sign_nr)
{
	if(sign_nr == SIGINT)
	{
		printf("\nClosing server ...\n");

        close(server);

	    printf("Done!\n");

        exit(0);
	}
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char* argv[])
{
    // parse_arguments()
    // configs according to arguments
    argc = argc;
    argv = argv;

    if(signal(SIGINT, signal_handler) == SIG_ERR)
    {
    	LOG_ERROR("Eroare la signal");
    	return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        LOG_ERROR("Eroare la creare socket");
        return -1;
    }

    int enable = 1;

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        LOG_ERROR("Eroare la socket option");
        goto close_server_socket;
    }

    server = server_socket;

    if (bind(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        LOG_ERROR("Eroare la bind");
        goto close_server_socket;
        return -1;
    }

    if (listen(server_socket, BACK_LOG) == -1)
    {
        LOG_ERROR("Eroare la listen");
        goto close_server_socket;
        return -1;
    }

    struct worker_type workers[MAX_THREADS];

    init_poll(workers);

    printf("Server running on port %d\n", SERVER_PORT);

    int client_socket = -1;

    while(1)
    {
    	client_socket = accept(server_socket, NULL, NULL);
    	dispatch_client(workers, client_socket);
    }

close_server_socket:
    close(server_socket);
    return -1;
}
