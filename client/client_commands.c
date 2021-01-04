#include "client_commands.h"

server_func functions[] =  
{
	NULL,
	create_account,
	login,
	get_file,
	send_file,
	ls,
	cd
};

unsigned int functions_number = LENGTH_OF(functions);

struct create_account_args
{
	int cmd_index;
	int restricted;
	char username[30];
	char password[30];
};

int create_account(SOCKET socket, int index)
{
	struct create_account_args credentials;

	int ok;

	printf("username: ");
	if(scanf(" %s", credentials.username) <= 0)
	{
		LOG_ERROR("Error at getting the username");
		return -1;
	}
	
	printf("\npassword: ");
	if(scanf(" %s", credentials.password) <= 0)
	{
		LOG_ERROR("Error at getting the password");
		return -1;
	}

	printf("\nrestricted: ");
	if(scanf(" %d", &credentials.restricted) <= 0)
	{
		LOG_ERROR("Error at getting the restricted");
		return -1;
	}

	credentials.cmd_index = index;

	int result = send_all(socket, (const char*)&credentials, sizeof(credentials));

	if(result == -1)
	{
		LOG_ERROR("Error at sending credentials");
		return -1;
	}

	ok = is_ok(socket);

	if(!ok)
	{
		int result = print_error(socket);

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}
		if(result < 0)
		{
			LOG_ERROR("Error at receiving error from server");
			return -1;
		}

		return 0;
	}
	else
	{
		printf("\n\nAccount created with success\n\n");
	}

	return 0;
}

struct login_args
{
	int cmd_index;
	char username[30];
	char password[30];
};

int login(SOCKET socket, int index)
{
	struct login_args credentials;

	int ok;

	do
	{
		printf("username: ");
		if(scanf(" %s", credentials.username) <= 0)
		{
			LOG_ERROR("Error at getting the username");
			return -1;
		}
		
		printf("\npassword: ");
		if(scanf(" %s", credentials.password) <= 0)
		{
			LOG_ERROR("Error at getting the password");
			return -1;
		}

		credentials.cmd_index = index;

		int result = send_all(socket, (const char*)&credentials, sizeof(credentials));

		if(result == -1)
		{
			LOG_ERROR("Error at sending credentials");
			return -1;
		}

		ok = is_ok(socket);

		if(!ok)
		{
			int result = print_error(socket);

			if(result == 0)
			{
				LOG_MSG("Server disconnected");
				return -1;
			}
			if(result < 0)
			{
				LOG_ERROR("Error at receiving error from server");
				return -1;
			}

			return 0;
		}
		else
		{
			printf("\n\nLogin success\n\n");
		}

	}while(!ok);

	return 0;
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

int get_file(SOCKET socket, int index)
{
	char buffer[BUFFER_SIZE];

    struct send_file_args cmd;

    cmd.cmd_index = index;

    printf("The file from server: ");
    if(fgets(cmd.file_path, 256, stdin) == NULL)
    {
    	LOG_ERROR("Error at getting the file path");
    	return -1;
    }

    cmd.file_path[strlen(cmd.file_path) - 1] = '\0';

 	char nume_fisier[256];

 	printf("\nPath where to save: ");
 	 if(fgets(nume_fisier, 256, stdin) == NULL)
 	{
 		LOG_ERROR("Error at getting the file name");
 		return -1;
 	}
 	printf("\n");

 	nume_fisier[strlen(nume_fisier) - 1] = '\0';

    int result = send_all(socket, (const char*)&cmd, sizeof(cmd));

    if(result < 0)
    {
    	LOG_ERROR("Error at sending to server");
    	return -1;
    }

	if(!is_ok(socket))
	{
		int result = print_error(socket);

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}
		if(result < 0)
		{
			LOG_ERROR("Error at receiving error from server");
			return -1;
		}

		return 0;
	}

    struct file_info fi;

 	result = recv_all(socket, (char*)&fi, sizeof(fi));

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

    while(fi.file_size > 0)
    {
    	if((unsigned long)time(NULL) - tm >= 1)
    	{
    		tm = (unsigned long)time(NULL);
    		printf("Remaining: ");
    	}

    	if(!is_ok(socket))
    	{
			int result = print_error(socket);

			if(result == 0)
			{
				LOG_MSG("Server disconnected");
				return -1;
			}
			if(result < 0)
			{
				LOG_ERROR("Error at receiving error from server");
				return -1;
			}

			return 0;
    	}

    	int length_to_recv = 0;

    	result = recv_all(socket, (char*)&length_to_recv, sizeof(length_to_recv));

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

		int len = recv_all(socket, buffer, length_to_recv);

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
    	if(write(fd, buffer, length_to_recv) <= 0)

#elif _WIN32
    	DWORD back;
    	if(WriteFile(fd, buffer, length_to_recv, &back, NULL) == 0)
#endif
    	{
    		LOG_ERROR("Error at writing in file");
    		goto close_file;
    	}
    	fi.file_size -= length_to_recv;
    }

    CloseHandle(fd);
    return 0;

close_file:
	CloseHandle(fd);

	remove(nume_fisier);

	return -1;
}

int send_file(SOCKET socket, int index)
{
	char buffer[BUFFER_SIZE];

    struct send_file_args cmd;

    cmd.cmd_index = index;

    printf("Path where to save on server: ");
     if(fgets(cmd.file_path, 256, stdin) == NULL)
    {
    	LOG_ERROR("Error at getting the file path");
    	return -1;
    }

    cmd.file_path[strlen(cmd.file_path) - 1] = '\0';

 	char nume_fisier[256];

 	printf("\nPath to the file to send: ");
 	if(fgets(nume_fisier, 256, stdin) == NULL)
 	{
 		LOG_ERROR("Error at getting the file name");
 		return -1;
 	}
 	printf("\n");

 	nume_fisier[strlen(nume_fisier) - 1] = '\0';

#ifdef __linux__
    if(access(nume_fisier, F_OK ))
#elif _WIN32
    if(PathFileExistsA(nume_fisier))
#endif
    {
    	LOG_MSG("File does not exists");
        return 0;
    }

    int result = send_all(socket, (const char*)&cmd, sizeof(cmd));

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

 	if(!is_ok(socket))
 	{
 		int result = print_error(socket);

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}
		if(result < 0)
		{
			LOG_ERROR("Error at receiving error from server");
			return -1;
		}

 		return 0;
 	}

#ifdef __linux__
 	int fd = open(nume_fisier, O_RDONLY);

 	if(fd == -1)
 	{
 		set_error(socket, "Error at open file");
 		return -1;
 	}
#elif _WIN32
	HANDLE fd = CreateFileA(
			nume_fisier,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

 	if(fd == INVALID_HANDLE_VALUE)
 	{
 		LOG_ERROR("Error at opening the file");
 		return -1;
 	}
#endif
    struct file_info fi;

    fi.file_type = 0;

#ifdef __linux__
    fi.file_size = lseek(fd, 0L, SEEK_END);
    lseek(fd, 0L, SEEK_SET);
#elif _WIN32
    DWORD high_order = 0;
    DWORD low_order = GetFileSize(fd, &high_order);
	fi.file_size = high_order;
	fi.file_size = fi.file_size << 32 | low_order;
#endif

	if(send_ok(socket) < 0)
	{
		LOG_ERROR("Error at sending ok");
		goto close_file;
	}

 	result = send_all(socket, (char*)&fi, sizeof(fi));

 	if(result < 0)
 	{
 		LOG_ERROR("Error at sending file info");
 		goto close_file;
 	}

 	while(fi.file_size > 0)
 	{
#ifdef __linux__
 		int len = read(fd, buffer, BUFFER_SIZE);
#elif _WIN32
 		INT len = 0;
 		if(!ReadFile(fd, buffer, BUFFER_SIZE, (DWORD*)&len, NULL))
 			len = -1;
#endif
 		if(len < 0)
 		{
 			set_error(socket, "Error at reading from file");
 			goto close_file;
 		}

 		if(send_ok(socket) < 0)
 		{
 			LOG_ERROR("Error at sending ok");
 			goto close_file;
 		}

 		result = send_all(socket, (const char*)&len, sizeof(len));

 		if(result < 0)
 		{
 			LOG_ERROR("Error at sending data from file");
 			goto close_file;
 		}

 		result = send_all(socket, buffer, len);

 		if(result < 0)
 		{
 			LOG_ERROR("Error at sending data from file");
 			goto close_file;
 		}

 		fi.file_size -= result;
 	}
 	
 	printf("Done\n");
 	CloseHandle(fd);
 	return 0;

close_file:
	CloseHandle(fd);
	return -1;
}

struct ls_args
{
	int index;
	int path;
};

int ls(SOCKET socket, int index)
{
	struct ls_args args;
	args.index = index;
	args.path = index;

	int result = send_all(socket, (const char*)&args, sizeof(args));

	if(result != sizeof(args))
		return -1;

	if(!is_ok(socket))
	{
		int result = print_error(socket);

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}
		if(result < 0)
		{
			LOG_ERROR("Error at receiving error from server");
			return -1;
		}

		return 0;
	}

	size_t length = 0;

	result = recv_all(socket, (char*)&length, sizeof(length));

	if(result <= 0)
		return -1;

	char buffer[BUFFER_SIZE] = {0};

	do
	{
		result = recv(socket, buffer, BUFFER_SIZE, 0);

		if(result <= 0)
			return -1;

		printf("%s", buffer);

		length -= result;

	}while(length > 0);

	return 0;
}

struct cd_args
{
	int index;
	char path[256];
};

int cd(SOCKET socket, int index)
{
	struct cd_args args;
	args.index = index;

	printf("path: ");
	if(fgets(args.path, 256, stdin) == NULL)
	{
		LOG_ERROR("Error at getting the path");
		return -1;
	}

	args.path[strlen(args.path) - 1] = '\0';

	int result = send_all(socket, (const char*)&args, sizeof(args));

	if(result < 0)
		return -1;

	if(!is_ok(socket))
	{
		int result = print_error(socket);

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}
		if(result < 0)
		{
			LOG_ERROR("Error at receiving error from server");
			return -1;
		}
	}

	return 0;
}

