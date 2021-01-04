#include "server_commands.h"

// the index of command is the position in the commands_list

struct client_function commands_list[] = {
	{SECURITY_ALL, get_command_new, get_command, NULL}, // always needs to be first
    {SECURITY_ALL, create_account_new, create_account, NULL},
	{SECURITY_ALL, login_new, login, NULL},
    {SECURITY_CONNECTED, send_file_new, send_file,  send_file_free},
    {SECURITY_CONNECTED, get_file_new, get_file, get_file_free},
    {SECURITY_CONNECTED, ls_new, ls, ls_free},
    {SECURITY_CONNECTED, cd_new, cd, NULL}
};

unsigned int max_cmds = LENGTH_OF(commands_list);

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
		
		if(cmd->ok == -1)
			client->data.current_command.cmd_state = COMMAND_ERROR;
	}

	return res;
}

int send_ok(struct entry* client)
{
	struct command* cmd = &client->data.current_command;

	if(cmd->ok_finished == 1)
		return 1;

	cmd->ok = 0;

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

struct data_type
{
	int read_len;
	char buffer[DATA_TRANSFER_CHUNK];
};

struct transfer_info
{
	int file_descriptor;
    struct data_type data;
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
	int need_delete;
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

        char buffer[MAX_PATH];
        strcpy(buffer, args->file_path);
        strcpy(args->file_path, client->data.working_directory);
        strcat(args->file_path, "/");
        strcat(args->file_path, buffer);

        printf("%s\n", args->file_path);

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

        if(flock(fd, LOCK_EX) == -1)
        {
            set_error(client, "Can't lock the file");
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
        	cmd->ok_size = 0;
        }

        return CLIENT_AGAIN;
    }
    
    if(args->tinfo.state == 2)
    {
        args->tinfo.data.read_len = read(args->tinfo.file_descriptor, args->tinfo.data.buffer, DATA_TRANSFER_CHUNK);

        if(args->tinfo.data.read_len < 0)
        {
            set_error(client, "Error at reading from file");
            return CLIENT_AGAIN;
        }

        if(args->tinfo.data.read_len > 0)
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

        int len = send(client->data.socket, &args->tinfo.data + args->buf_size, (args->tinfo.data.read_len + sizeof(int)) - args->buf_size, 0);

        if(len == -1)
        {
            LOG_ERROR("Can't send the message");
            return CLIENT_ERROR;
        }

        args->buf_size += len;

        cmd->ok_finished = 0;
        cmd->ok_size = 0;

        if(args->buf_size == (args->tinfo.data.read_len + (int)sizeof(int)))
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
	struct file_send_args* args = client->data.args;

	if(args->tinfo.file_descriptor != 0)
	{
		flock(args->tinfo.file_descriptor, LOCK_UN);
		close(args->tinfo.file_descriptor);
	}
}


int get_file_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct file_send_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result get_file(struct entry* client, enum client_events event)
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

        char buffer[MAX_PATH];
        strcpy(buffer, args->file_path);
        strcpy(args->file_path, client->data.working_directory);
        strcat(args->file_path, "/");
        strcat(args->file_path, buffer);

        if(access(args->file_path, F_OK ) == 0)
        {
            set_error(client, "File already exists");
            return CLIENT_WANT_WRITE;
        }
   
        int fd = open(args->file_path, O_CREAT | O_WRONLY, S_IRWXU);

        if(fd == -1)
        {
            set_error(client, "Can't create the file");
            return CLIENT_WANT_WRITE;
        }

        if(flock(fd, LOCK_EX) == -1)
        {
            set_error(client, "Can't lock the file");
            return CLIENT_WANT_WRITE;
        }

        args->tinfo.file_descriptor = fd;
        args->tinfo.state = 1;

        args->finfo.file_type = 0;
        args->finfo.file_size = 0;
        args->buf_size = 0;

        return CLIENT_WANT_WRITE;
    }

    if(args->tinfo.state == 1)
    {
    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    		{
    			args->need_delete = 1;
    			return CLIENT_ERROR;
    		}
    		return CLIENT_AGAIN;
    	}

    	cmd->ok_finished = 0;
    	cmd->ok_size = 0;
    	args->tinfo.state = 2;

    	return CLIENT_WANT_READ;
    }

	if(args->tinfo.state == 2)
    {
    	if(!cmd->ok_finished)
    	{
    		int result = get_ok(client);

    		if(cmd->ok_finished && cmd->ok == -1)
    			args->need_delete = 1;

    		if(result < 0)
    		{
    			args->need_delete = 1;
    			return CLIENT_ERROR;
	        }

    		if(result == 0)
    		{
	        	args->need_delete = 1;
    			return CLIENT_CLOSED;
    		}

    		return CLIENT_AGAIN;
    	}

        int result = recv(client->data.socket, &args->finfo + args->buf_size, sizeof(struct file_info) - args->buf_size, 0);

        if(result < 0)
        {
    		args->need_delete = 1;
           	return CLIENT_ERROR;
        }

        if(result == 0)
    	{
        	args->need_delete = 1;
    		return CLIENT_CLOSED;
    	}

        args->buf_size += result;

        if(args->buf_size == sizeof(struct file_info))
        {
        	args->buf_size = 0;
        	args->tinfo.state = 3;
        	cmd->ok_finished = 0;
      		cmd->ok_size = 0;
        }

        return CLIENT_AGAIN;
    }
    
    if(args->tinfo.state == 3)
    {
		if(!cmd->ok_finished)
    	{
    		int result = get_ok(client);

    		if(cmd->ok_finished && cmd->ok == -1)
    			args->need_delete = 1;

    		if(result < 0)
    		{
    			args->need_delete = 1;
    			return CLIENT_ERROR;
    		}

    		if(result == 0)
    		{
    			args->need_delete = 1;
    			return CLIENT_CLOSED;
    		}

    		return CLIENT_AGAIN;
    	}

        int len = recv(client->data.socket, &args->tinfo.data.read_len + args->buf_size, sizeof(args->tinfo.data.read_len) - args->buf_size, 0);

        if(len < 0)
        {
        	args->need_delete = 1;
            LOG_ERROR("Can't recv the message");
            return CLIENT_ERROR;
        }

        if(len == 0)
        {
        	args->need_delete = 1;
        	return CLIENT_CLOSED;
        }

        args->buf_size += len;

        if(args->buf_size == sizeof(args->tinfo.data.read_len))
        {
        	args->buf_size = 0;
        	args->tinfo.state = 4;
	        cmd->ok_finished = 0;
        }

        return CLIENT_AGAIN;
    }

    if(args->tinfo.state == 4)
    {
    	int len = recv(client->data.socket, args->tinfo.data.buffer + args->buf_size, args->tinfo.data.read_len - args->buf_size, 0);

        if(len < 0)
        {
        	args->need_delete = 1;
            LOG_ERROR("Can't recv the message");
            return CLIENT_ERROR;
        }

        if(len == 0)
        {
        	args->need_delete = 1;
        	return CLIENT_CLOSED;
        }

        args->buf_size += len;

        if(args->buf_size == args->tinfo.data.read_len)
        {
        	args->tinfo.state = 5;
        	args->buf_size = 0;
        }

        return CLIENT_AGAIN;
    }

    if(args->tinfo.state == 5)
    {
    	int written_len = write(args->tinfo.file_descriptor, args->tinfo.data.buffer, args->tinfo.data.read_len);

	    if(written_len != args->tinfo.data.read_len)
	    {
	    	LOG_ERROR("Error at writing in file");
	    	args->need_delete = 1;
	    	return CLIENT_ERROR;
	    }

    	args->finfo.file_size -= args->tinfo.data.read_len;

    	if(args->finfo.file_size <= 0)
    	{
    		args->need_delete = 0;
    		return CLIENT_SUCCESS;
    	}

    	args->tinfo.state = 3;

    	return CLIENT_AGAIN;
    }

    return CLIENT_ERROR;
}

void get_file_free(struct entry* client)
{
	struct file_send_args* args = client->data.args;

	if(args->tinfo.file_descriptor != 0)
	{
		flock(args->tinfo.file_descriptor, LOCK_UN);
		close(args->tinfo.file_descriptor);
	}

	if(args->need_delete == 1)
	{
		remove(args->file_path);
	}

}

static int have_rights(const char* ip, const char* line)
{
	const char* begin = line;

	for(int i = 0; i < 3; i++)
	{
		begin = strchr(begin, '/');

		if(begin == NULL)
			return -1;

		begin++;
	}

	if(strstr(begin, "wl") == begin)
		return 1;
	else if(strstr(begin, "bl") != begin)
		return -1;

	begin = line;

	for(int i = 0; i < 2; i++)
	{
		begin = strchr(begin, '/');

		if(begin == NULL)
			return -1;

		begin++;
	}

	char* buffer = strstr(begin, ip);

	if(buffer == begin)
	{
		return 1;
	}

	return 0;
}

static char* get_line_from_str(const char* string, size_t length)
{
	char* buffer = strndup(string, length + 1);
	if(buffer != NULL)
		buffer[length] = '\0';

	return buffer;
}

static int account_exists(struct entry* client, const char* username, const char* password)
{
	static const char delimiter = '\n';
	pthread_mutex_lock(&accounts_mutex);

	const char* it = accounts.buffer;

	size_t patter_len = strlen(username) + strlen(password) + 2;

	char* pattern = calloc(1, patter_len + 1);

	if(pattern == NULL)
	{
		pthread_mutex_unlock(&accounts_mutex);
		return -1;
	}

	strcat(pattern, username);
	strcat(pattern, "/");
	strcat(pattern, password);
	strcat(pattern, "/");

	if(strlen(pattern) != patter_len)
	{
		pthread_mutex_unlock(&accounts_mutex);
		free(pattern);
		return -1;
	}

	do
	{
		const char* found = strchr(it, delimiter);

		if(found == NULL)
			found = accounts.buffer + accounts.length - 1;

		char* line = get_line_from_str(it, found - it);

		if(line == NULL)
		{
			free(pattern);
			pthread_mutex_unlock(&accounts_mutex);
			return -1;
		}

		char* buffer = strstr(line, pattern);

		if(buffer == line)
		{
			free(pattern);
			int result = have_rights(client->data.ip, line);

			pthread_mutex_unlock(&accounts_mutex);
			free(line);
			
			if(result <= 0)
				return result;

			if(line[found - it - 1] == 'a')
				client->data.client_security = SECURITY_ADMIN;
			else
				client->data.client_security = SECURITY_CONNECTED;

			return 1;
		}

		free(line);
		it = found + 1;

	}while(it - accounts.buffer < accounts.length);

	pthread_mutex_unlock(&accounts_mutex);

	return 0;
}

struct user_pass
{
	char username[30];
	char password[30];
};

struct login_args
{
	int buf_size;
	struct user_pass credentials;
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
			int result = account_exists(client, args->credentials.username, args->credentials.password);

			if(result == -1)
			{
				set_error(client, "Some error occured");
				return CLIENT_WANT_WRITE;
			}

			if(!result)
				set_error(client, "Wrong credentials");
			else
			{
				strcpy(client->data.working_directory, CLIENTS_DIRECTORY);
				strcat(client->data.working_directory, "/");
				strcat(client->data.working_directory, args->credentials.username);

				free(client->data.username);

				client->data.username = strdup(args->credentials.username);

				if(client->data.username == NULL)
					set_error(client, "Some error occured");

				free(client->data.home_dir);

				client->data.home_dir = strdup(client->data.working_directory);

				if(client->data.username == NULL)
					set_error(client, "Some error occured");
			}

			return CLIENT_WANT_WRITE;
		}
	}

	if(event == EVENT_WRITE)
	{
		struct command* cmd = &client->data.current_command;

    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

    	cmd->ok_finished = 0;
    	return CLIENT_SUCCESS;
	}

	return CLIENT_ERROR;
}

static int username_exists(const char* username)
{
	static const char delimiter = '\n';
	const char* it = accounts.buffer;

	do
	{
		const char* found = strchr(it, delimiter);

		if(found == NULL)
			found = accounts.buffer + accounts.length -1;

		char* line = get_line_from_str(it, found - it);

		char* buffer = strstr(line, username);

		if(buffer != NULL)
		{
			if(buffer == line)
			{
				free(line);
				return 1;
			}
		}

		free(line);
		it = found + 1;

	}while(it - accounts.buffer < accounts.length);

	return 0;
}

static int add_account(const char* username, const char* password, const char* ip, int restricted)
{
	size_t line_length = strlen(username) + strlen(password) + strlen(ip) + 8;
	char* line = calloc(1, line_length + 1);

	if(line == NULL)
	{
		LOG_ERROR("Error at alocating memory");
		return -1;
	}

	strcat(line, username);
	strcat(line, "/");
	strcat(line, password);
	strcat(line, "/");
	strcat(line, ip);
	strcat(line, "/");

	if(restricted)
	{
		strcat(line, "bl");
	}
	else
	{
		strcat(line, "wl");
	}

	strcat(line, "/n\n");

	if(strlen(line) != line_length)
	{
		LOG_ERROR("Error at concatenating username and password");
		goto free_line;
	}

	int f = open(ACCOUNTS_FILE, O_WRONLY);

	if(f == -1)
	{
		LOG_ERROR("Error at opening accounts file");
		goto free_line;
	}

	if(lseek(f, 0, SEEK_END) == (off_t)-1)
	{
		LOG_ERROR("Error at moving cursor");
		close(f);
		goto free_line;
	}

	size_t written = write(f, line, line_length);
	free(line);
	close(f);

	if(written != line_length)
	{
		LOG_ERROR("Error at writing in file");
		return -1;
	}

	return 0;

free_line:
	free(line);
	return -1;
}

struct create_account_user_pass
{
	int restricted;
	char username[30];
	char password[30];
};

struct create_account_args
{
	int buf_size;
	struct create_account_user_pass credentials;
};

int create_account_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct create_account_args));

	if(client->data.args == NULL)
		return -1;
	
	return 0;
}

enum client_result create_account(struct entry* client, enum client_events event)
{
	struct create_account_args* args = client->data.args;

	if(event == EVENT_READ)
	{
		int len = recv(client->data.socket, &args->credentials + args->buf_size, sizeof(args->credentials) - args->buf_size, 0);

		if(len == 0)
			return CLIENT_CLOSED;

		if(len < 0)
			return CLIENT_ERROR;
		
		args->buf_size += len;

		if(args->buf_size == sizeof(struct create_account_user_pass))
		{
			pthread_mutex_lock(&accounts_mutex);

			int response = username_exists(args->credentials.username);

			if(response == -1)
			{
				pthread_mutex_unlock(&accounts_mutex);
				set_error(client, "Some error occured, try again");
				return CLIENT_WANT_WRITE;
			}

			if(response)
			{
				pthread_mutex_unlock(&accounts_mutex);
				set_error(client, "Username already exists");
				return CLIENT_WANT_WRITE;
			}

			munmap(accounts.buffer, accounts.length);

			if(add_account(args->credentials.username, args->credentials.password, client->data.ip, args->credentials.restricted) != 0)
			{
				pthread_mutex_unlock(&accounts_mutex);
				set_error(client, "Some error occured, try again");
				return CLIENT_WANT_WRITE;
			}

			accounts = get_accounts(ACCOUNTS_FILE);
			pthread_mutex_unlock(&accounts_mutex);

			if(accounts.buffer == NULL || accounts.length == 0)
				set_error(client, "Some error occured, try again");

			args->buf_size = 0;

			char buffer[MAX_PATH] = CLIENTS_DIRECTORY;
			strcat(buffer, "/");
			strcat(buffer, args->credentials.username);

			struct stat status = {0};

			if (stat(buffer, &status) == -1)
			{
				mkdir(buffer, 0700);
			}

			return CLIENT_WANT_WRITE;
		}
	}
	else if(event == EVENT_WRITE)
	{
		struct command* cmd = &client->data.current_command;

    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

    	cmd->ok_finished = 0;
    	return CLIENT_SUCCESS;
	}

	return CLIENT_ERROR;
}

static size_t get_files(const char* path, char** buffer)
{
	DIR* directory;

	struct dirent* dir;

	directory = opendir(path);

	if(directory == NULL)
		return -1;

	size_t length = 0;
	size_t allocated = 0;

	static const size_t chunk = 40;

	while((dir = readdir(directory)) != NULL)
	{
		size_t file_name_len = strlen(dir->d_name) + 1;

		int need_resize = 0;

		while(allocated - length < file_name_len + 1)
		{
			allocated += chunk;
			need_resize = 1;
		}

		if(need_resize)
		{
			char* buffer_new = realloc(*buffer, allocated);

			if(buffer_new == NULL)
			{
				free(*buffer);
				return -1;
			}

			if(*buffer == NULL)
				*buffer_new = '\0';

			*buffer = buffer_new;
		}

		strcat(*buffer, dir->d_name);
		strcat(*buffer, "\n");

		length += file_name_len;
	}

	closedir(directory);

	return length;
}

struct ls_data
{
	size_t length;
	size_t sended;
	char* buffer;
};

struct ls_args
{
	int state;
	struct ls_data data;
};

int ls_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct ls_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result ls(struct entry* client, enum client_events event)
{
	(void) event;

	struct ls_args* args = client->data.args;

	if(args->state == 0)
	{
		char* buffer = NULL;

		int length = get_files(client->data.working_directory, &buffer);

		if(length == -1)
		{
			set_error(client, "Some error occured");
			return CLIENT_WANT_WRITE;
		}

		int path;
		int result = recv(client->data.socket, &path, sizeof(path), 0);

		if(result == 0)
			return CLIENT_CLOSED;

		if(result < 0)
			return CLIENT_ERROR;

		args->data.length = length + 1;
		args->data.buffer = buffer;
		args->state = 1;

		return CLIENT_WANT_WRITE;
	}

	if(args->state == 1)
	{
		struct command* cmd = &client->data.current_command;

    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

    	int result = send(client->data.socket, &args->data.length + args->data.sended, sizeof(args->data.length) - args->data.sended, 0);

    	if(result < 0)
    		return CLIENT_ERROR;

    	args->data.sended += result;

    	if(args->data.sended == sizeof(args->data.length))
    	{
    		args->data.sended = 0;
    		args->state = 2;
    	}

    	return CLIENT_AGAIN;
	}

	if(args->state == 2)
	{
		int result = send(client->data.socket, args->data.buffer + args->data.sended, args->data.length - args->data.sended, 0);

		if(result < 0)
			return CLIENT_ERROR;

		args->data.sended += result;

		if(args->data.sended == args->data.length)
			return CLIENT_SUCCESS;
	}

	return CLIENT_ERROR;
}

void ls_free(struct entry* client)
{
	struct ls_args* args = client->data.args;
	free(args->data.buffer);
}

static int directory_back(char* path)
{
	char* delimiter = strrchr(path, '/');

	if(delimiter == NULL)
		return -1;

	*delimiter = '\0';

	return 1;
}

static int change_wd(struct entry* client, char* path)
{
	if(strcmp(path, "..") == 0)
	{
		if(strcmp(client->data.working_directory, client->data.home_dir) == 0)
			return 0;

		int result = directory_back(client->data.working_directory);

		return result;
	}

	if(strstr(path, "/../") != NULL || strstr(path, "/./") != NULL)
		return 0;

	char buffer[MAX_PATH];

	strcpy(buffer, client->data.working_directory);
	strcat(buffer, "/");
	strcat(buffer, path);

	struct stat status = { 0 };

    if(stat(buffer, &status) == -1)
    {
    	return 0;
    }

   	strcpy(client->data.working_directory, buffer);

	return 1;
}

struct cd_args
{
	int state;
	char path[MAX_PATH];
	size_t recived;
};

int cd_new(struct entry* client)
{
	client->data.args = calloc(1, sizeof(struct cd_args));

	if(client->data.args == NULL)
		return -1;

	return 0;
}

enum client_result cd(struct entry* client, enum client_events event)
{
	(void)event;

	struct cd_args* args = client->data.args;
	if(args->state == 0)
	{
		int result = recv(client->data.socket, args->path + args->recived, MAX_PATH - args->recived, 0);

		if(result < 0)
			return CLIENT_ERROR;

		if(result == 0)
			return CLIENT_CLOSED;

		args->recived += result;

		if(args->recived == MAX_PATH)
		{
			args->state = 1;
			return CLIENT_WANT_WRITE;
		}

		return CLIENT_AGAIN;
	}

	if(args->state == 1)
	{
		int result = change_wd(client, args->path);

		if(result == -1)
		{
			set_error(client, "Some error occured");
			return CLIENT_WANT_WRITE;
		}

		if(!result)
		{
			set_error(client, "Invalid path");
			return CLIENT_WANT_WRITE;
		}
		
		args->state = 2;
		return CLIENT_WANT_WRITE;
	}

	if(args->state == 2)
	{
		struct command* cmd = &client->data.current_command;

    	if(!cmd->ok_finished)
    	{
    		if(send_ok(client) < 0)
    			return CLIENT_ERROR;
    		return CLIENT_AGAIN;
    	}

    	return CLIENT_SUCCESS;
	}

	return CLIENT_ERROR;
}