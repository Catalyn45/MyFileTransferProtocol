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
#include "myVector.h"

#define BACK_LOG 10
#define BUFFER_CHUNK 1024
#define SERVER_PORT 8089
#define MAX_THREADS 20

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

#define EVENT_FUN(fun_name) \
    void fun_name(int client_socket, void* args)

fd_set* sockets;
int server;

enum event_type
{
    read_event,
    write_event
};

typedef void (*socket_event_cb)(int client_socket, void* args);


EVENT_FUN(socket_read)
{

}

EVENT_FUN(socket_write)
{

}

static void socket_write

struct socket_event
{
    enum event_type type;

    int socket;

    int state;

    socket_event_cb read_fun;
    socket_event_cb write_fun;
    socket_event_cb timeout_fun;

    void* args;
};

struct entry
{
	struct socket_event data;
	STAILQ_ENTRY(entry) entries;
};

void* handle_client(void* args)
{
    int client = *((int *)args);
    client = client;
    free(args);
    return NULL;
}

int init_poll(pthread_t *threads, struct events_head* queue_of_ready_clients)
{
    int i;
    for(i = 0; i < MAX_THREADS; i++)
    {
        if(pthread_create(&threads[i], NULL, handle_client, queue_of_ready_clients) == -1)
            break;
    }

    if(i != MAX_THREADS)
    {
        for(int j = 0; j < i; j++)
        {
            pthread_exit(&threads[j]);
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

		printf("Closing sockets ...\n");

        if(sockets != NULL)
    	{
            for(int i = 0; i < FD_SETSIZE; i++)
    	        if(FD_ISSET(i, sockets))
                {
                    printf("client closed\n");
                    close(i);
                }
        }
        else
        {
            close(server);
        }

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

    printf("Server running on port %d\n", SERVER_PORT);
    
    fd_set current_set, copy_set;
    FD_ZERO(&current_set);
    
    sockets = &current_set;

    FD_SET(server_socket, &current_set);

    STAILQ_HEAD(events_head, entry);

    struct events_head events;

    STAILQ_INIT(&events);

    pthread_t threads[MAX_THREADS];

    init_poll(&threads, &events);

    while(1)
    {
        read_set = current_set;
    	write_set = current_set;

        if(select(FD_SETSIZE, &read_set, &write_set, NULL, NULL) == -1)
        {
            LOG_ERROR("Eroare la select");
            goto close_sockets;
        }

        for(int i = 0; i < FD_SETSIZE; i++)
        {
            if(FD_ISSET(i, &read_set))
            {
                if(i == server_socket)
                {
                    struct sockaddr_in client_adress;
                    socklen_t size = sizeof(client_adress);

                    int client = accept(i, (struct sockaddr*)&client_adress, &size);

                    FD_SET(client, &current_set);

                    printf(
                        "New client connected to server ip: %s port %d\n", 
                        inet_ntoa(client_adress.sin_addr),
                        ntohs(client_adress.sin_port)
                    );
                }
                else
                {
                	struct entry client_event* = malloc(sizeof(struct entry));
                	client_event->data.type = read_event;
                	pthread_mutex_lock(&mutex);
                	STAILQ_INSERT_TAIL(&events, client_event, entries);
                	pthread_mutex_unlock(&mutex);

                }
            }
            else if (FD_ISSET(i, &write_set))
            {
	    	    struct entry client_event* = malloc(sizeof(struct entry));
				client_event->data.type = write_event;
	        	pthread_mutex_lock(&mutex);
	        	STAILQ_INSERT_TAIL(&events, client_event, entries);
	        	pthread_mutex_unlock(&mutex);
            }

        }
    }

close_sockets:
    FD_CLR(server_socket, &current_set);
    for(int i = 0; i < FD_SETSIZE; i++)
        if(FD_ISSET(i, &current_set))
            close(i);
close_server_socket:
    close(server_socket);

    return -1;
}
