#include "server_commands.h"

struct client_function commands_list[] = {
	{SECURITY_ALL, get_command_new, get_command, get_command_free},
	{SECURITY_ALL, login_new, login, login_free},
    {SECURITY_CONNECTED, send_file_new, send_file,  send_file_free}
};

unsigned int max_cmds = LENGTH_OF(commands_list);


struct command_args
{
	int command_index;
	unsigned int buf_size;
};

int get_command_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct command_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result get_command(struct entry* client, enum client_events event)
{
	(void) event;

	struct command_args* args = client->data.args;

	if(event == EVENT_READ)
	{
		int len = recv(client->data.socket, (&args->command_index) + args->buf_size, sizeof(int) - args->buf_size, 0);

		if(len == -1)
			return CLIENT_ERROR;

		if(len == 0)
			return CLIENT_CLOSED;

		args->buf_size += len;

		if(args->buf_size < sizeof(int))
			return CLIENT_AGAIN;

		return CLIENT_SUCCESS;
	}

	return CLIENT_WANT_READ;
}

void get_command_free(struct entry* client)
{
	free(client->data.args);
}

struct transfer_info
{
	int file_descriptor;
    data_type data;
    int read_len;
	int state;
};

struct file_info
{
	int ok;
	off_t file_size;
	int file_type;
};

struct data_type
{
	int ok;
	char buffer[DATA_TRANSFER_CHUNK];
}

struct file_send_args
{
	struct transfer_info tinfo;
	struct file_info finfo;
	int buf_size;
	char file_path[MAX_PATH];
};

int send_file_new(struct entry* client)
{
	client->data.args = malloc(sizeof(struct file_send_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result send_file(struct entry* client, enum client_events event)
{
	(void) event;

    struct file_send_args* args = client->data.args;

	if (args->tinfo.state == 0)
    {

    	int len = recv(client->data.socket, args->file_path + args->buf_size, MAX_PATH - args->buf_size, 0);

    	if(len == 0)
    		return CLIENT_CLOSED;

    	if(len < 0)
    		return CLIENT_ERROR;

        args->buf_size += len;

        if(args->buf_size < MAX_PATH)
        	return CLIENT_AGAIN;

        
        int fd = open(args->file_path, O_RDONLY);

        if(fd == -1)
        {
            LOG_ERROR("Can't open this file");
            return CLIENT_ERROR;
        }

        args->tinfo.file_descriptor = fd;
        args->tinfo.state = 1;

        args->finfo.file_type = 0;
        args->finfo.file_size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0L, SEEK_SET);
        args->buf_size = 0;

        //aici o sa mai trimit la client daca totul e ok si poate primi fisierul

        return CLIENT_WANT_WRITE;
	    
    }

	if(args->tinfo.state == 1)
    {

        int result = send(client->data.socket, &args->finfo + args->buf_size, sizeof(struct file_info) - args->buf_size, 0);

        if(result == -1)
        {
            LOG_ERROR("Can't send to client");
            return CLIENT_ERROR;
        }

        args->buf_size += result;

        if(args->buf_size == sizeof(struct file_info))
        {
        	args->buf_size = 0;
        	args->tinfo.state = 2;
        }

        return CLIENT_AGAIN;
    }
    
    if(args->tinfo.state == 2)
    {
        args->tinfo.read_len = read(args->tinfo.file_descriptor, args->tinfo.buffer, DATA_TRANSFER_CHUNK);

        if(args->tinfo.read_len < 0)
        {
            LOG_ERROR("Error at reading from file");
            return CLIENT_AGAIN;
        }

        if(args->tinfo.read_len > 0)
        {
            args->tinfo.state = 3;
            return CLIENT_AGAIN;
        }

        return CLIENT_SUCCESS;
    }

    if(args->tinfo.state == 3)
    {
        int len = send(client->data.socket, args->tinfo.buffer + args->buf_size, args->tinfo.read_len - args->buf_size, 0);

        if(len == -1)
        {
            LOG_ERROR("Can't send the message");
            return CLIENT_ERROR;
        }

        args->buf_size += len;

        if(args->buf_size == args->tinfo.read_len)
        {
        	args->buf_size = 0;
        	args->tinfo.state = 2;
        }

        return CLIENT_AGAIN;
    }

    return CLIENT_ERROR;
}

void send_file_free(struct entry* client)
{
	(void)client;
}


struct user_pass
{
	char username[30];
	char password[30];
};

struct login_response
{
	int ok;
};

struct login_args
{
	int buf_size;
	struct user_pass credentials;
	struct login_response response;
};

int login_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct login_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result login(struct entry* client, enum client_events event)
{
	static const char* username = "admin";
	static const char* password = "admin";

	struct login_args* args = client->data.args;

	if(event == EVENT_READ)
	{
		int len = recv(client->data.socket, &args->credentials + args->buf_size, sizeof(args->credentials) - args->buf_size, 0);

		if(len == 0)
			return CLIENT_CLOSED;

		if(len < 0)
			return CLIENT_ERROR;
		
		args->buf_size += len;

		if(args->buf_size == sizeof(struct user_pass))
		{
			if(strcmp(args->credentials.username, username) == 0 && strcmp(args->credentials.password, password) == 0)
			{
				client->data.client_security = SECURITY_CONNECTED;
				args->response.ok = 1;
			}

			args->buf_size = 0;
			return CLIENT_WANT_WRITE;
		}
	}

	if(event == EVENT_WRITE)
	{
		int len = send(client->data.socket, &args->response + args->buf_size, sizeof(args->response) - args->buf_size, 0);

		if(len == -1)
		{
			return CLIENT_ERROR;
		}

		args->buf_size += len;

		if(args->buf_size == sizeof(args->response))
			return CLIENT_SUCCESS;
	}

	return CLIENT_ERROR;
}

void login_free(struct entry* client)
{
	free(client->data.args);
}