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

#define BACK_LOG 10
#define BUFFER_CHUNK 1024
#define SERVER_PORT 8089

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

fd_set* sockets;
int server;

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

    while(1)
    {
        copy_set = current_set;
    
        if(select(FD_SETSIZE, &copy_set, NULL, NULL, NULL) == -1)
        {
            LOG_ERROR("Eroare la select");
            goto close_sockets;
        }

        for(int i = 0; i < FD_SETSIZE; i++)
        {
            if(FD_ISSET(i, &copy_set))
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
                    char buffer[BUFFER_CHUNK];

                    int result = recv(i, buffer, BUFFER_CHUNK - 1, 0);

                    if(result <= 0)
                    {
                        struct sockaddr_in client_adress;
                        socklen_t size = sizeof(client_adress);

                        getpeername(i, (struct sockaddr*)&client_adress, &size);

                        printf(
                            "client disconnected from server ip: %s port %d\n", 
                            inet_ntoa(client_adress.sin_addr),
                            ntohs(client_adress.sin_port)
                        );

                        printf("conn closed\n");
                        fflush(stdout);
                        close(i);
                        FD_CLR(i, &current_set);
                    }
                    else
                    {
                        buffer[result] = '\0';
                        printf("%s", buffer);
                        fflush(stdout);
                    }
                }
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
