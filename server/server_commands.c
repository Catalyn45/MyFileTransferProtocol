#include "server_commands.h"

struct client_function commands_list[] = {
	{SECURITY_ALL, get_command_new, get_command, get_command_free},
	{SECURITY_ALL, login_new, login, login_free},
    {SECURITY_CONNECTED, send_file_new, send_file,  send_file_free}
};

unsigned int max_cmds = LENGTH_OF(commands_list);

int set_error(struct entry* client, const char* message)
{
	client->data.current_command.cmd_state = COMMAND_ERROR;

	client->data.error.buffer = strdup(message);
	client->data.error.error_length = strlen(message) + 1;

	if(client->data.error.buffer == NULL)
		return -1;

	return 0;
}

int get_ok(struct entry* client)
{
	struct command* cmd = &client->data.current_command;

	if(cmd->ok_finished == 1)
		return 1;

	int res = recv(client->data.socket, &cmd->ok + cmd->ok_size, sizeof(cmd->ok) - cmd->ok_size, 0);

	if(res <= 0)
		return res;

	cmd->ok_size += res;

	if(cmd->ok_size == sizeof(cmd->ok))
	{
		cmd->ok_finished = 1;
		cmd->ok_size = 0;
	}

	return res;
}

int send_ok(struct entry* client)
{
	struct command* cmd = &client->data.current_command;

	if(cmd->ok_finished == 1)
		return 1;

	int res = send(client->data.socket, &cmd->ok + cmd->ok_size, sizeof(cmd->ok) - cmd->ok_size, 0);

	if(res < 0)
		return res;

	cmd->ok_size += res;

	if(cmd->ok_size == sizeof(cmd->ok))
	{
		cmd->ok_finished = 1;
		cmd->ok_size = 0;
	}

	return res;
}

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
	(void)client;
}

struct data_type
{
	char buffer[DATA_TRANSFER_CHUNK];
};

struct transfer_info
{
	int file_descriptor;
    struct data_type data;
    int read_len;
	int state;
};

struct file_info
{
	off_t file_size;
	int file_type;
};

struct file_send_args
{
	struct transfer_info tinfo;
	struct file_info finfo;
	int buf_size;
	char file_path[MAX_PATH];
};


int send_file_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct file_send_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result send_file(struct entry* client, enum client_events event)
{
	(void) event;

    struct file_send_args* args = client->data.args;
    struct command* cmd = &client->data.current_command;

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

        if(access(args->file_path, F_OK ) != 0)
        {
            set_error(client, "File does not exists");
            return CLIENT_WANT_WRITE;
        }
   
        int fd = open(args->file_path, O_RDONLY);

        if(fd == -1)
        {
            set_error(client, "Can't open the file");
            return CLIENT_WANT_WRITE;
        }

        args->tinfo.file_descriptor = fd;
        args->tinfo.state = 1;

        args->finfo.file_type = 0;
        args->finfo.file_size = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0L, SEEK_SET);
        args->buf_size = 0;

        return CLIENT_WANT_WRITE;
    }

	if(args->tinfo.state == 1)
    {

    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

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
        	cmd->ok_finished = 0;
        }

        return CLIENT_AGAIN;
    }
    
    if(args->tinfo.state == 2)
    {
        args->tinfo.read_len = read(args->tinfo.file_descriptor, args->tinfo.data.buffer, DATA_TRANSFER_CHUNK);

        if(args->tinfo.read_len < 0)
        {
            set_error(client, "Error at reading from file");
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
    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

        int len = send(client->data.socket, args->tinfo.data.buffer + args->buf_size, args->tinfo.read_len - args->buf_size, 0);

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
        	cmd->ok_finished = 0;
        }

        return CLIENT_AGAIN;
    }

    return CLIENT_ERROR;
}

void send_file_free(struct entry* client)
{
	struct file_send_args* args = client->data.args;

	if(args->tinfo.file_descriptor != 0)
		close(args->tinfo.file_descriptor);
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
	(void)client;
}