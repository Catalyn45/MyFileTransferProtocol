#include <stdio.h>
#include <sys/select.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char* argv[])
{
    // parse_arguments()
    // configs according to arguments
    argc = argc;
    argv = argv;
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8089);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        //eroare
        return -1;
    }

    if (bind(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        //eroare
        goto close_server_socket;
        return -1;
    }

    if (listen(server_socket, 10) == -1)
    {
        //eroare
        goto close_server_socket;
        return -1;
    }
    
    fd_set current_set, copy_set;

    FD_ZERO(&current_set);
    
    FD_SET(server_socket, &current_set);

    while(1)
    {
        copy_set = current_set;
    
        if(select(FD_SETSIZE, &copy_set, NULL, NULL, NULL) == -1)
        {
            //eroare
            goto close_sockets;
        }

        for(int i = 0; i < FD_SETSIZE; i++)
        {
            if(FD_ISSET(i, &copy_set))
            {
                if(i == server_socket)
                {
                    int client = accept(i, NULL, NULL);
                    FD_SET(client, &current_set);
                }
                else
                {
                    char buffer[1024];
                    int result = recv(i, buffer, 1023, 0);

                    if(result <= 0)
                    {
                        printf("conn closed");
                        close(i);
                        FD_CLR(i, &current_set);
                    }
                    else
                    {
                        buffer[result] = '\0';
                        printf("%s", buffer);
                    }
                }
            }
        }
    }

close_sockets:
    for(int i = 0; i < FD_SETSIZE; i++)
        if(FD_ISSET(i, &current_set))
            close(i);
close_server_socket:
    close(server_socket);

    return -;
}
