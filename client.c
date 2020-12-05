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
#include <sys/queue.h>
#include <dirent.h>

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

struct command
{
	char args[1024];
	int index;
};

int main()
{
	struct sockaddr_in server_address;
    
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(8089);
    inet_pton(AF_INET, "127.0.0.1", &(server_address.sin_addr));

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        LOG_ERROR("Eroare la creare socket");
        return -1;
    }


    if(connect(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
    	LOG_ERROR("Error at connect");
    	close(server_socket);
    	return -1;
    }

    char buffer[1024];

    recv(server_socket, buffer, 1024, 0);

    printf("%s\n", buffer);


    struct command cmd;

    cmd.index = 0;
    strcpy(cmd.args, ".");

    send(server_socket, &cmd, sizeof(cmd), 0);

    recv(server_socket, buffer, 1024, 0);

    printf("%s\n", buffer);

    close(server_socket);

	return 0;
}