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
#include <time.h>

#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))

#define BUFFER_SIZE 1024 * 32

struct command
{
	char args[1024];
	int index;
};

struct file_info
{
	off_t file_size;
	int file_type;
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

    char buffer[BUFFER_SIZE];

    recv(server_socket, buffer, BUFFER_SIZE, 0);

    printf("%s\n", buffer);


    struct command cmd;

    cmd.index = 1;
    strcpy(cmd.args, "test.zip");

    send(server_socket, &cmd, sizeof(cmd), 0);

    struct file_info fi;

 	recv(server_socket, &fi, sizeof(fi), 0);

 	int fd = open("transfered_file.zip", O_CREAT | O_WRONLY);

 	unsigned long tm = (unsigned long)time(NULL);
 	off_t last_bytes_remaining = fi.file_size;

    while(fi.file_size > 0)
    {
    	if((unsigned long)time(NULL) - tm >= 1)
    	{
    		tm = (unsigned long)time(NULL);
    		printf("%ld KB/s\n", (last_bytes_remaining - fi.file_size) / 1000);
    		printf("Remaining: %ld\n", fi.file_size / 1000);
    		last_bytes_remaining = fi.file_size;
    	}

    	int len = recv(server_socket, buffer, BUFFER_SIZE, 0);
    	write(fd, buffer, len);
    	fi.file_size -= len;
    }

    close(fd);


    close(server_socket);

	return 0;
}