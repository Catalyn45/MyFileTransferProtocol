#ifndef TYPES_H
#define TYPES_H

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
#include <Shlwapi.h>
#define size_t unsigned long long
#endif 

typedef int (*server_func)(SOCKET socket, int index);

#ifdef __linux__
#define LOG_ERROR(message) \
    printf("%s\n%s\n%d\n\n", message, strerror(errno), __LINE__)
#elif _WIN32
#define LOG_ERROR(message) \
    printf("%s\nError nr: %ld\n\n", message, GetLastError())
#endif

#define LOG_MSG(message) \
    printf("%s\n", message)

#define LENGTH_OF(x) sizeof(x) / sizeof(*(x))

#define BUFFER_SIZE 1024 * 32


struct error_type
{
	int length;
	char* buffer;
};

ssize_t send_all(SOCKET socket, const char* buffer, size_t length);
ssize_t recv_all(SOCKET socket, char* buffer, size_t length);
int is_ok(SOCKET socket);
int send_ok(SOCKET socket);
int print_error(SOCKET socket);
int set_error(SOCKET socket, const char* msg);

#endif