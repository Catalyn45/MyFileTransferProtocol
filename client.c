#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __linux__
#define SOCK_ERROR -1
#define closesocket close
#define CloseHandle close
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
#define PORT 8090
#define MAX_IP_LEN 256

#ifdef __linux__
#define LOG_ERROR(message) \
    printf("%s\n%s\n\n", message, strerror(errno))
#elif _WIN32
#define LOG_ERROR(message) \
    printf("%s\nError nr: %d\n\n", message, GetLastError())
#endif

#define BUFFER_SIZE 1024 * 32

struct file_info
{
	off_t file_size;
	int file_type;
};

struct login_args
{
	int cmd_index;
	char username[30];
	char password[30];
};

struct send_file_args
{
	int cmd_index;
	char file_path[256];
};

struct response
{
	int ok;
};

int login(int socket)
{
	struct login_args credentials;

	strcpy(credentials.username, "admin");
	strcpy(credentials.password, "admin");
	credentials.cmd_index = 1;

	send(socket, &credentials, sizeof(credentials), 0);

	struct response resp;

	recv(socket, &resp, sizeof(resp), 0);

	return resp.ok;
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
    server_address.sin_port = htons(8089);

#ifdef _WIN32
    SOCKET server_socket;
    server_address.sin_addr.s_addr = inet_addr(IP);
#elif __linux__
    inet_pton(AF_INET, IP, &(server_address.sin_addr));
    int server_socket;
#endif

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == SOCK_ERROR)
    {
        LOG_ERROR("Eroare la creare socket");
        return -1;
    }


    if(connect(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) < 0)
    {
    	LOG_ERROR("Error at connect");
    	closesocket(server_socket);
    	return -1;
    }

    login(server_socket);

    char buffer[BUFFER_SIZE];

    struct send_file_args cmd;

    printf("Insert command: \n");
    scanf("%d", &cmd.cmd_index);
    scanf("%s", cmd.file_path);

    send(server_socket, (const char*)&cmd, sizeof(cmd), 0);

    struct file_info fi;

 	recv(server_socket, (char*)&fi, sizeof(fi), 0);

 	char nume_fisier[256];

 	printf("Dai numele cum sa se salveze fisieru, fara extensie ca ii pun eu\n");
 	scanf("%s", nume_fisier);
 	strcat(nume_fisier, ".rar");

#ifdef __linux__
 	int fd = open(nume_fisier, O_CREAT | O_WRONLY);
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
#endif

 	unsigned long tm = (unsigned long)time(NULL);
 	off_t last_bytes_remaining = fi.file_size;
 	off_t initial_size = fi.file_size;

    while(fi.file_size > 0)
    {
    	if((unsigned long)time(NULL) - tm >= 1)
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
#ifdef __linux__
    	write(fd, buffer, len);
#elif _WIN32
    	DWORD back;
    	WriteFile(fd, buffer, len, &back, NULL);
#endif
    	fi.file_size -= len;
    }

    CloseHandle(fd);

    closesocket(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif

	return 0;
}
