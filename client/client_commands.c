#include "client_commands.h"
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>

server_func functions[] =  
{
	NULL,
	send_hint,
	create_account,
	login,
	get_file,
	send_file,
	ls,
	cd,
	help
};

unsigned int functions_number = LENGTH_OF(functions);

void get_random_string(unsigned char buffer[30], size_t* out_len)
{
	srand(time(NULL));

	size_t length = rand()%10 + 21;

	for(int i = 0; i < 30; i++)
		buffer[i] = rand() % 256;

	*out_len = length;
}

static int generate_key(unsigned char* string, size_t length, unsigned char out_key[32])
{
    unsigned char output[16];
    if(mbedtls_md5_ret(string, length, output) != 0)
        return -1;

    if(mbedtls_sha256_ret(output, 16, out_key, 0) != 0)
        return -1;

    return 0;
}


static void crypt_msg(const unsigned char* key, unsigned char* message, size_t message_len)
{
    for(size_t i = 0; i < message_len; i++)
    {
        for(size_t j = 0; j < 32; j++)
        {
            message[i] = message[i] ^ key[j];
        }
    }
}

static int encode_password(const char* password, size_t password_len, char out[44])
{
	unsigned char output[16];

    if(mbedtls_md5_ret((const unsigned char*)password, password_len, output) != 0)
        return -1;

    unsigned char sha_out[32];
    if(mbedtls_sha256_ret(output, 16, sha_out, 0) != 0)
        return -1;

    size_t out_len;
    if(mbedtls_base64_encode((unsigned char*)out, 50, &out_len, sha_out, 32) != 0)
    	return -1;

    return 0;
}

struct create_account_data
{
	int restricted;
	char username[30];
	char password[44];
};

struct create_account_args
{
	int crypted;
	struct create_account_data credentials;
};

int crypted_conn;
unsigned char crypted_key[32];

int create_account(SOCKET socket, int index)
{
	struct create_account_args args;

	int ok;

	printf("username: ");
	if(scanf(" %s", args.credentials.username) <= 0)
	{
		LOG_ERROR("Error at getting the username");
		return 0;
	}
	
	char password[30];
	printf("\npassword: ");
	if(scanf(" %s", password) <= 0)
	{
		LOG_ERROR("Error at getting the password");
		return 0;
	}

	printf("\ncrypted: ");
	if(scanf(" %d", &args.crypted) <= 0)
	{
		LOG_ERROR("Error at getting the restricted");
		return 0;
	}

	printf("\nrestricted: ");
	if(scanf(" %d", &args.credentials.restricted) <= 0)
	{
		LOG_ERROR("Error at getting the restricted");
		return 0;
	}
	getchar();

	if(encode_password(password, strlen(password), args.credentials.password) != 0)
		return 0;

	if(args.crypted)
	{
		if(!crypted_conn)
		{
			int result = send_hint(socket, 1);

			if(result == -1)
			{
				LOG_ERROR("Error at send_hint");
				return -1;
			}

			if(result == 0)
				return 0;;

			crypted_conn = 1;
		}

		crypt_msg(crypted_key, (unsigned char*)&args.credentials, sizeof(args.credentials));
	}

	int result = send_all(socket, (const char*)&index, sizeof(index));

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

	result = send_all(socket, (const char*)&args, sizeof(args));

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

struct key_hint
{
	unsigned char buffer[30];
	size_t length;
};

int send_hint(SOCKET socket, int index)
{
	struct key_hint args;

	get_random_string(args.buffer, &args.length);

	int result = send_all(socket, (const char*)&index, sizeof(index));

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

	result = send_all(socket, (const char*)&args, sizeof(args));

	if(result < 0)
	{
		LOG_ERROR("Error at sending to server");
		return result;
	}

	int ok = is_ok(socket);

	if(!ok)
	{
		result = print_error(socket);

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

	result = generate_key(args.buffer, args.length, crypted_key);

	if(result == -1)
		return -1;

	return 1;
}

struct login_data
{
	char username[30];
	char password[44];
};

struct login_args
{
	int crypted;
	struct login_data credentials;
};

int login(SOCKET socket, int index)
{
	struct login_args args;

	int ok;

	do
	{
		printf("username: ");
		if(scanf(" %s", args.credentials.username) <= 0)
		{
			LOG_ERROR("Error at getting the username");
			return -1;
		}
		
		char password[30];

		printf("\npassword: ");
		if(scanf(" %s", password) <= 0)
		{
			LOG_ERROR("Error at getting the password");
			return -1;
		}

		printf("\ncrypted: ");
		if(scanf(" %d", &args.crypted) <= 0)
		{
			LOG_ERROR("Error at getting the password");
			return -1;
		}

		getchar();

		if(encode_password(password, strlen(password), args.credentials.password) != 0)
		{	
			LOG_ERROR("Error at encoding");
			return -1;
		}

		if(args.crypted)
		{
			if(!crypted_conn)
			{
				int result = send_hint(socket, 1);

				if(result == -1)
				{
					LOG_ERROR("Error at send_hint");
					return -1;
				}

				if(result == 0)
					return 0;

				crypted_conn = 1;
			}

			crypt_msg(crypted_key, (unsigned char*)&args.credentials, sizeof(args.credentials));
		}

		int result = send_all(socket, (const char*)&index, sizeof(index));

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

		result = send_all(socket, (const char*)&args, sizeof(args));

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

 	int result = send_all(socket, (const char*)&index, sizeof(index));

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

    result = send_all(socket, (const char*)&cmd, sizeof(cmd));

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
 		return 0;
 	}

    if(flock(fd, LOCK_EX) == -1)
    {
        LOG_ERROR("Can't lock the file");
        goto close_file;
    }
#elif _WIN32
 	HANDLE fd = CreateFileA(
 			nume_fisier,
 			GENERIC_READ | GENERIC_WRITE,
 			0,
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

#ifdef __linux__
    		printf("Remaining: %ld\n", fi.file_size);
#elif _WIN32
			printf("Remaining: %lld\n", fi.file_size);
#endif
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

    printf("Done\n");

    return 0;

close_file:
	CloseHandle(fd);

#ifdef __linux__
	remove(nume_fisier);
#elif _WIN32
	DeleteFile(nume_fisier);
#endif
	return 0;
}

int send_file(SOCKET socket, int index)
{
	char buffer[BUFFER_SIZE];

    struct send_file_args cmd;

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
    if(access(nume_fisier, F_OK ) != 0)
#elif _WIN32
    if(!PathFileExistsA(nume_fisier))
#endif
    {
    	LOG_MSG("File does not exists");
        return 0;
    }

    int result = send_all(socket, (const char*)&index, sizeof(index));

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

    result = send_all(socket, (const char*)&cmd, sizeof(cmd));

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
 		return 0;
 	}

    if(flock(fd, LOCK_EX) == -1)
    {
        set_error(socket, "Can't lock the file");
        goto close_file;
    }
#elif _WIN32
	HANDLE fd = CreateFileA(
			nume_fisier,
			GENERIC_READ | GENERIC_WRITE,
			0,
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

 	unsigned long tm = (unsigned long)time(NULL);

 	while(fi.file_size > 0)
 	{

    	if((unsigned long)time(NULL) - tm >= 1)
    	{
    		tm = (unsigned long)time(NULL);
#ifdef __linux__
			printf("Remaining: %ld\n", fi.file_size);
#elif _WIN32
			printf("Remaining: %lld\n", fi.file_size);
#endif
    	}
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
close_file:
	CloseHandle(fd);
	return 0;
}

struct ls_args
{
	int path;
};

int ls(SOCKET socket, int index)
{
	struct ls_args args;
	args.path = index;

	int result = send_all(socket, (const char*)&index, sizeof(index));

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

	result = send_all(socket, (const char*)&args, sizeof(args));

	if(result < 0)
	{	
		LOG_ERROR("Error at sending args");
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

	size_t length = 0;

	result = recv_all(socket, (char*)&length, sizeof(length));

	if(result == 0)
	{
		LOG_MSG("Server disconnected");
		return -1;
	}

	if(result < 0)
	{
		LOG_ERROR("Error at server");
		return -1;
	}

	char buffer[BUFFER_SIZE] = {0};

	do
	{
		result = recv(socket, buffer, BUFFER_SIZE - 1, 0);

		buffer[result] = '\0';

		if(result == 0)
		{
			LOG_MSG("Server disconnected");
			return -1;
		}

		if(result < 0)
		{
			LOG_ERROR("Error at server");
			return -1;
		}

		printf("%s", buffer);

		length -= result;

	}while(length > 0);

	return 0;
}

struct cd_args
{
	char path[256];
};

int cd(SOCKET socket, int index)
{
	struct cd_args args;

	printf("path: ");
	if(fgets(args.path, 256, stdin) == NULL)
	{
		LOG_ERROR("Error at getting the path");
		return -1;
	}

	args.path[strlen(args.path) - 1] = '\0';

	int result = send_all(socket, (const char*)&index, sizeof(index));

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

	result = send_all(socket, (const char*)&args, sizeof(args));

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

int help(SOCKET socket, int index)
{
	(void)socket;
	(void)index;

	printf(
	"help -> Shows all the commands\n"\
	"\n"\
	"create_account -> Create an account on the server\n"\
	"\tParameters:\n"\
	"\t\tusername - username of the user\n"\
	"\t\tpassword - the password of the user\n"\
	"\t\trestricted - 1 if account can be accesed only with creation ip or 0 otherwise\n"\
	"\t\tcrypted - 1 if credentials want to be send crypted to the server or 0 otherwise\n"\
	"\n"\
	"login -> login on the server\n"\
	"\tParameters:\n"\
	"\t\tusername - username of the user\n"\
	"\t\tpassword - the password of the user\n"\
	"\t\tcrypted - 1 if credentials want to be send crypted to the server or 0 otherwise\n"\
	"\n"\
	"send_hint -> send a hint to the server to create a crypted connection\n"\
	"\n"\
	"ls -> Show the file from the current directory\n"\
	"\n"\
	"cd -> Change current directory\n"\
	"\tParameters:\n"\
	"\t\tpath - path to the directory to be changed\n"\
	"\n"
	"get_file -> get a file from the server\n"\
	"\tParameters:\n"\
	"\t\tpath_from_server - path to the file to get\n"\
	"\t\tpath_to_save - path to where to save the file\n"\
	"\n"\
	"get_file -> get a file from the server\n"\
	"\tParameters:\n"\
	"\t\tpath_to_server - path to the file to save\n"\
	"\t\tpath_to_send - path to the file to be sended\n"\
	);

	return 0;
}