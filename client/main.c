#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __linux__
#define SOCK_ERROR -1
#define closesocket close
#define CloseHandle close
#define SOCKET int
#include <sys/select.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/queue.h>
#include <dirent.h>
#include <arpa/inet.h>
#elif _WIN32
#define SOCK_ERROR INVALID_SOCKET
#define off_t long long
#include <windows.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#endif 

#define IP "127.0.0.1"
#define PORT 8089
#define MAX_IP_LEN 256

#ifdef __linux__
#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))
#elif _WIN32
#define LOG_ERROR(message) \
    printf("%s\nError nr: %d\n\n", message, GetLastError())
#endif

#define LOG_MSG(message) \
    printf("%s\n", message)

#define LENGTH_OF(x) sizeof(x) / sizeof(*(x))

#define BUFFER_SIZE 1024 * 32

struct login_args
{
	int cmd_index;
	char username[30];
	char password[30];
};

struct response
{
	int ok;
};

int login(SOCKET socket)
{
	struct response resp;

	struct login_args credentials;

	do
	{
		printf("username: ");
		if(scanf("%s", credentials.username) <= 0)
		{
			LOG_ERROR("Error at getting the username");
			return -1;
		}
		
		printf("\npassword: ");

		if(scanf("%s", credentials.password) <= 0)
		{
			LOG_ERROR("Error at getting the password");
			return -1;
		}

		credentials.cmd_index = 1;

		int result = send(socket, (const char*)&credentials, sizeof(credentials), 0);

		if(result == -1)
		{
			LOG_ERROR("Error at sending credentials");
			return -1;
		}

		result = recv(socket, (char*)&resp, sizeof(resp), 0);

		if(result == 0)
		{
			LOG_MSG("Server closed connection");
			return -1;
		}

		if(result < 0)
		{
			LOG_ERROR("Error at receiving response from server");
			return -1;
		}

		if(resp.ok != 1)
			printf("\n\nLogin failed\n\n");
		else
			printf("\n\nLogin success\n\n");

	}while(resp.ok != 1);

	return resp.ok;
}

struct send_file_args
{
	int cmd_index;
	char file_path[256];
};

struct file_info
{
	off_t file_size;
	int file_type;
};

int receive_file(SOCKET server_socket)
{
	char buffer[BUFFER_SIZE];

    struct send_file_args cmd;

    cmd.cmd_index = 2;

    if(scanf("%s", cmd.file_path) <= 0)
    {
    	LOG_ERROR("Error at getting the file path");
    	return -1;
    } 

    int result = send(server_socket, (const char*)&cmd, sizeof(cmd), 0);

    if(result == 0)
    {
    	LOG_MSG("Server closed");
    	return -1;
    }

    if(result < 0)
    {
    	LOG_ERROR("Error at receiving from server");
    	return -1;
    }

    struct file_info fi;

 	result = recv(server_socket, (char*)&fi, sizeof(fi), 0);

	if(result == 0)
    {
    	LOG_MSG("Server closed");
    	return -1;
    }

    if(result < 0)
    {
    	LOG_ERROR("Error at receiving from server");
    	return -1;
    }

 	char nume_fisier[256];

 	if(scanf("%s", nume_fisier) <= 0)
 	{
 		LOG_ERROR("Error at getting the file name");
 		return -1;
 	}

#ifdef __linux__
 	int fd = open(nume_fisier, O_RDWR | O_CREAT, S_IRWXU);

 	if(fd == -1)
 	{
 		LOG_ERROR("Error at opening the file");
 		return -1;
 	}
#elif _WIN32
 	HANDLE fd = CreateFileA(
 			nume_fisier,
 			GENERIC_READ | GENERIC_WRITE,
 			FILE_SHARE_READ,
 			NULL,
 			CREATE_ALWAYS,
 			FILE_ATTRIBUTE_NORMAL,
 			NULL
 		);
 	if(fd == INVALID_HANDLE_VALUE)
 	{
 		LOG_ERROR("Error at opening the file");
 		return -1;
 	}
#endif

 	unsigned long tm = (unsigned long)time(NULL);
 	off_t last_bytes_remaining = fi.file_size;
 	off_t initial_size = fi.file_size;

    while(fi.file_size > 0)
    {
    	if((unsigned long)time(NULL) - tm >= 1 || initial_size == fi.file_size)
    	{
    		tm = (unsigned long)time(NULL);

#ifdef _WIN32
    		printf("%lld KB/s (%lld MB/s)\n", (last_bytes_remaining - fi.file_size) / (off_t)1000, (last_bytes_remaining - fi.file_size) / (off_t)1000000);
    		printf("Downloaded: %lld%%\n", ((off_t)100 - (fi.file_size * (off_t)100) / initial_size));
#elif __linux__
    		printf("%ld KB/s (%ld MB/s)\n", (last_bytes_remaining - fi.file_size) / (off_t)1000, (last_bytes_remaining - fi.file_size) / (off_t)1000000);
    		printf("Downloaded: %ld%%\n", ((off_t)100 - (fi.file_size * (off_t)100) / initial_size));
#endif
    		last_bytes_remaining = fi.file_size;
    	}

    	int len = recv(server_socket, buffer, BUFFER_SIZE, 0);

    	if(len == 0)
	    {
	    	LOG_MSG("Server closed");
	    	goto close_file;
	    }

	    if(len < 0)
	    {
	    	LOG_ERROR("Error at receiving from server");
	    	goto close_file;
	    }

#ifdef __linux__
    	if(write(fd, buffer, len) <= 0)

#elif _WIN32
    	DWORD back;
    	if(WriteFile(fd, buffer, len, &back, NULL) == 0)
#endif
    	{
    		LOG_ERROR("Error at writing in file");
    		goto close_file;
    	}
    	fi.file_size -= len;
    }

    printf("Downloaded: 100%%\n");

    CloseHandle(fd);
    return 0;

close_file:
	CloseHandle(fd);

	return -1;

}

typedef int (*server_func)(SOCKET socket);

server_func functions[] =  
{
	NULL,
	login,
	receive_file
};

unsigned int functions_number = LENGTH_OF(functions);

int execute_command(unsigned int index, SOCKET socket)
{
	if(index == 0)
		return -1;

	if(index >= functions_number)
		return -1;

	return functions[index](socket);
}

int parse_command(const char* command)
{
	static const char* commands[] = {
		NULL,
		"login",
		"get_file"
	};

	for(unsigned int i = 1; i < LENGTH_OF(commands); i++)
	{
		if(strcmp(command, commands[i]) == 0)
			return i;
	}

	return -1;
}

int main(int argc, char* argv[])
{
	char ip[MAX_IP_LEN] = IP;
	unsigned short port = PORT;

	port = PORT;
	if(argc > 1)
	{
		strcpy(ip, argv[1]);
	}

	if(strcmp(ip, "localhost") == 0)
	{
		strcpy(ip, "127.0.0.1");
	}

	if(argc > 2)
	{
		port = (unsigned short)atoi(argv[2]);
	}

	#ifdef _WIN32
	WSADATA wsaData;

	int result = WSAStartup(MAKEWORD(2,2), &wsaData);

    if (result != 0)
    {
        LOG_ERROR("Error at wsaStartup");
        return 1;
    }
#endif

	struct sockaddr_in server_address;
    
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    SOCKET server_socket;
#ifdef _WIN32
    server_address.sin_addr.s_addr = inet_addr(ip);
#elif __linux__
    inet_pton(AF_INET, ip, &(server_address.sin_addr));
#endif

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == SOCK_ERROR)
    {
        LOG_ERROR("Eroare la creare socket");
        return -1;
    }


    if(connect(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
    	printf("%s\n", ip);
    	printf("%d\n", port);
    	LOG_ERROR("Error at connect");
    	closesocket(server_socket);
    	return -1;
    }

    login(server_socket);

    char current_command[256];

    while(1)
    {
    	printf("Insert command: ");
	    if(scanf("%s", current_command) <= 0)
	    {
	    	LOG_ERROR("Error at getting the command");
	    	goto close_client;
	    }

	    printf("\n");

	    int cmd_index = parse_command(current_command);

	    if(cmd_index == -1)
	    {
	    	LOG_MSG("Error at parsing the command");
	    	goto close_client;
	    }

	    int result = execute_command(cmd_index, server_socket);

	    if(result == -1)
	    {
	    	LOG_MSG("Error at executing the command");
	    	goto close_client;
	    }
	}

    closesocket(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif

	return 0;

close_client:
	closesocket(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif
    return -1;
}
