#include "types.h"

#define IP "127.0.0.1"
#define PORT 8089
#define MAX_IP_LEN 256

extern server_func functions[];
extern unsigned int functions_number;

ssize_t send_all(SOCKET socket, const char* buffer, size_t length)
{
	size_t sended = 0;

	do
	{
		int result = send(socket, buffer + sended, length - sended, 0);

		if(result < 0)
			return result;

		sended += result;
	}while(sended < length);

	return sended;
}

ssize_t recv_all(SOCKET socket, char* buffer, size_t length)
{
	size_t recived = 0;

	do
	{
		int result = recv(socket, buffer + recived, length - recived, 0);

		if(result <= 0)
			return result;

		recived += result;
	}while(recived < length);

	return recived;
}

int is_ok(SOCKET socket)
{
	int ok = 0;

	recv_all(socket, (char*)&ok, sizeof(ok));

	if(ok != 0)
		return 0;

	return 1;
}

int send_ok(SOCKET socket)
{
	int ok = 0;

	int result = send_all(socket, (const char*)&ok, sizeof(int));

	return result;
}

int print_error(SOCKET socket)
{
	struct error_type error;

	int result = recv_all(socket, (char*)&error.length, sizeof(error.length));

	if(result <= 0)
		return result;

	error.buffer = malloc(error.length);

	result = recv_all(socket, error.buffer, error.length);

	if(result <= 0)
	{
		free(error.buffer);
		return result;
	}

	printf("%s\n", error.buffer);
	free(error.buffer);

	return 1;
}

int set_error(SOCKET socket, const char* msg)
{
	int ok = -1;
	int result = send_all(socket, (const char*)&ok, sizeof(int));

	if(result < 0)
		return result;

	unsigned int error_length = strlen(msg) + 1;

	result = send_all(socket, (const char*)&error_length, sizeof(error_length));

	if(result < 0)
		return result;

	result = send_all(socket, msg, error_length);

	return result;
}

int execute_command(unsigned int index, SOCKET socket)
{
	if(index == 0)
		return -1;

	if(index >= functions_number)
		return -1;

	return functions[index](socket, index);
}

int parse_command(const char* command)
{
	static const char* commands[] = {
		NULL,
		"create_account",
		"login",
		"get_file",
		"send_file",
		"ls",
		"cd"
	};

	for(unsigned int i = 1; i < LENGTH_OF(commands); i++)
	{
		if(strstr(command, commands[i]) == command)
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

	printf("%s\n", ip);

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

    char current_command[256];

    while(1)
    {
    	printf("Insert command: ");
	    if(fgets(current_command, 256, stdin) == NULL)
	    {
	    	LOG_ERROR("Error at getting the command");
	    	goto close_client;
	    }

	    printf("\n");

	    int cmd_index = parse_command(current_command);

	    if(cmd_index == -1)
	    {
	    	LOG_MSG("Invalid command");
	    	continue;
	    }

	    int result = execute_command(cmd_index, server_socket);

	    if(result == -1)
	    {
	    	LOG_MSG("Error at executing the command");
	    	goto close_client;
	    }

	    getchar();
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
