#include "server.h"

void delete_socket(struct slisthead* clients, int socket)
{

	struct entry* it;

	SLIST_FOREACH(it, clients, entries)
	{
		if(it->data.socket == socket)
		{
			SLIST_REMOVE(clients, it, entry, entries);
			close(it->data.socket);
			free(it);
			return;
		}
	}
}

int show_files(struct entry* client)
{
    int socket = client->data.socket;

    struct command* cmd = client->data.args;
    

    char buffer[BUFFER_CHUNK];
    buffer[0] = '\0';

    const char* path = cmd->args;

    DIR* d;

    struct dirent* dir;

    d = opendir(path);

    if(d == NULL)
    {
        const char* error_msg = "Unavaible path, sry";
        send_message(socket, error_msg, strlen(error_msg) + 1);
        return 0;
    }

    while((dir = readdir(d)) != NULL)
    {
        strcat(buffer, dir->d_name);
        strcat(buffer, "\n");
    }

    closedir(d);

    send_message(socket, buffer, strlen(buffer) + 1);

    FD_CLR(socket, client->data.write);

    return 0;
}

int send_file(struct entry* client)
{
    struct command* cmd = client->data.current_command;


    struct transfer_info* info = client->data.args;

    char buffer[DATA_TRANSFER_CHUNK];

    if (info == NULL)
    {
        info = malloc(sizeof(struct transfer_info));

        if(info == NULL)
        {
            LOG_ERROR("Cant alloc memory");
            return -1;
        }

        info->file_descriptor = open(cmd->args, O_RDONLY);

        struct file_info fi;
        fi.file_type = 0;

        fi.file_size = lseek(info->file_descriptor, 0L, SEEK_END);
        lseek(info->file_descriptor, 0L, SEEK_SET);

        info->finished = 0;

        client->data.args = (void*)info;

        send_message(client->data.socket, (const char*)&fi, sizeof(fi));
    }

    int len = read(info->file_descriptor, buffer, DATA_TRANSFER_CHUNK);

    if(len > 0)
    {
        send_message(client->data.socket, buffer, len);
    }
    else
    {
        close(info->file_descriptor);
        free(info);
        client->data.args = NULL;
        FD_CLR(client->data.socket, client->data.write);
    }

    return 0;
}

int execute_command(struct entry* client)
{
    if(client == NULL)
        return -1;

    struct command* cmd = client->data.current_command;

    if(cmd == NULL)
        return -1;

    switch(cmd->index)
    {
    case get_files_list:
        return show_files(client);
        break;
    case get_file:
        return send_file(client);
        break;
    default:
        break;
    }

    return 0;
}

int send_message(int socket, const char* message, int len)
{
    int total_sended = 0;
    int sended = 0;

    do
    {
        sended = send(socket, message + total_sended, len - total_sended, 0);

        if(sended == -1)
            return -1;

        total_sended += sended;

    }while(total_sended < len);

    return total_sended;
}

int recv_message(int socket, char* buffer, int expected_len)
{
    int total_recved = 0;
    int recved = 0;

    do
    {
        recved = recv(socket, buffer + total_recved, expected_len - total_recved, 0);

        if(recved <= 0)
            return recved;

        total_recved += recved;

    }while(total_recved < expected_len);

    return total_recved;
}

int insert_client(struct slisthead* clients, struct entry client)
{
	struct entry* client_info = malloc(sizeof(struct entry));

    if(client_info == NULL)
    {
        LOG_ERROR("Error at allocating memory");
        return -1;
    }

	*client_info = client;

	SLIST_INSERT_HEAD(clients, client_info, entries);

    return 0;
}

struct entry* get_element(struct slisthead* clients, int socket)
{
	struct entry* it;

	SLIST_FOREACH(it, clients, entries)
	{
		if(it->data.socket == socket)
		{
			return it;
		}
	}

	return NULL;
}

int socket_read(int client_socket, struct slisthead* clients, int index)
{
    struct entry* client = get_element(clients, client_socket);
    struct command* cmd = NULL;
    int len = 0;

    if(client->data.state == connected)
    {
        cmd = malloc(sizeof(struct command));

        if(cmd == NULL)
        {
            LOG_ERROR("Error at allocating memory");
            return -1;
        }

        len = recv_message(client_socket, (char*)cmd, sizeof(struct command));
    }

	if(len <= 0)
	{
		FD_CLR(client_socket, client->data.master);

		delete_socket(clients, client_socket);

		pthread_mutex_lock(&mutex);
		clients_connected[index] --;
		pthread_mutex_unlock(&mutex);

		LOG_MSG("Client disconnected");

        free(cmd);

        if(len < 0)
            LOG_ERROR("Error at socket read");
	}
    else
    {
        client->data.current_command = cmd;
        FD_SET(client_socket, client->data.write);
    }

    return len;
}

int socket_write(int client_socket, struct slisthead* clients, int index)
{

    (void) index;

    struct entry* client_info = get_element(clients, client_socket);

    if(client_info == NULL)
    {
        LOG_MSG("Client not in list");
        return -1;
    }

    int result = 0;

    switch(client_info->data.state)
    {
    case login:
        {
        	const char* msg = "Bine ati venit pe serverul ftp a lui Catalyn45!\n";
            result = send_message(client_socket, msg, strlen(msg) + 1);

            if(result == -1)
            {   
                LOG_ERROR("Error at sending message");
                return -1;
            }

            client_info->data.state = connected;
        	FD_CLR(client_socket, client_info->data.write);
        }
        break;
    case connected:
            execute_command(client_info);
        break;
    default:
        break;
    }

    return result;
}

void exit_thread(struct slisthead* clients, int index)
{
	struct entry* it;

	while (!SLIST_EMPTY(clients))
	{
    	it = SLIST_FIRST(clients);
        SLIST_REMOVE_HEAD(clients, entries);
        close(it->data.socket);
        free(it);
    }

    clients_connected[index] = -1;
}

void* handle_client(void* args)
{
    struct thread_arg main_thread = *((struct thread_arg *)args);
    free(args);
    int main_thread_socket = main_thread.socket;

    struct slisthead clients;
    SLIST_INIT(&clients);
    fd_set master_fd, read_fd, write_fd, copy_write_fd;
 
    FD_ZERO(&master_fd);
    FD_ZERO(&write_fd);

    FD_SET(main_thread_socket, &master_fd);

    while(1)
    {
    	read_fd = master_fd;
    	copy_write_fd = write_fd;

    	int count = select(FD_SETSIZE, &read_fd, &copy_write_fd, NULL, NULL);

        if(count < 0)
        {
            LOG_ERROR("Error at select, closing thread");
            exit_thread(&clients, main_thread.index);
            pthread_detach(workers[main_thread.index].thread_handle);
            return NULL;
        }

    	for(int i = 0; i < FD_SETSIZE; i++)
    	{
    		if(FD_ISSET(i, &read_fd))
    		{
    			if(i == main_thread_socket)
    			{
    				
    				int socket_to_add = 0;

    				if(recv_message(i, (char*)&socket_to_add, sizeof(int)) <= 0)
                    {
                        LOG_ERROR("Error at reciving new socket from mainthread");
                        exit_thread(&clients, main_thread.index);
                        pthread_detach(workers[main_thread.index].thread_handle);
                        return NULL;
                    }

    				if(socket_to_add == -1)
    				{
    					exit_thread(&clients, main_thread.index);
                        return NULL;
    				}

    				LOG_MSG("Client connected");

    				fflush(stdout);

    				FD_SET(socket_to_add, &master_fd);

    				struct entry client;
    				memset(&client, 0, sizeof(client));

    				client.data.socket = socket_to_add;
                    client.data.master = &master_fd;
                    client.data.write = &write_fd;
    				
    				if(insert_client(&clients, client) == -1)
                    {
                        LOG_ERROR("Error at putting the client in list");
                        exit_thread(&clients, main_thread.index);
                        pthread_detach(workers[main_thread.index].thread_handle);
                        return NULL;
                    }

                    FD_SET(socket_to_add, &write_fd);

    			}
    			else
    			{
    				if(socket_read(i, &clients, main_thread.index) <= 0)
                    {
                        if(SLIST_EMPTY(&clients))
                        {
                            LOG_MSG("no clients, closing the thread");
                            exit_thread(&clients, main_thread.index);
                            pthread_detach(workers[main_thread.index].thread_handle);
                            return NULL;
                        }
                    }
    			}
    		}
    		else if(FD_ISSET(i, &copy_write_fd))
    		{
    			socket_write(i, &clients, main_thread.index);
    		}
    	}

    }
    return NULL;
}

void dispatch_client(struct worker_type* workers, int client_socket)
{
	int index = 0;
    int min = -1;

    int i;

	pthread_mutex_lock(&mutex);

	for(i = 0; i < MAX_THREADS; i++)
	{
		if(clients_connected[i] == -1)
		{
            LOG_MSG("Creating new thread");

			if(init_thread(&workers[i]) == -1)
            {
                LOG_MSG("Failed to create the thread");
                continue;
            }

            clients_connected[i] = 0;
            index = i;
            break;
		}
        else if(min == -1 || clients_connected[i] < min)
        {
            index = i;
            min = clients_connected[i];
        }
	}

	pthread_mutex_unlock(&mutex);

    if(i == MAX_THREADS && min == -1)
    {
        LOG_MSG("Can't assign client to any thread, closing the socket...");
        close(client_socket);
        return;
    }

	if(send_message(workers[index].socket, (const char*)&client_socket, sizeof(int)) == -1)
    {
        LOG_ERROR("Can't send the socket to thread");
        return;
    }
}

int init_thread(struct worker_type* worker)
{
	int sockp[2];

	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)
	{
        LOG_ERROR("Error at creating socketpair for thread");
        return -1;
    }

	worker->socket = sockp[0];

	struct thread_arg* worker_arg = malloc(sizeof(struct thread_arg));

    if(worker_arg == NULL)
    {
        LOG_ERROR("Error at allocating memory for worker_arg");
        goto close_socketpair;
    }

	worker_arg->socket = sockp[1];

    if(pthread_create(&worker->thread_handle, NULL, handle_client, worker_arg) == -1)
    {
        LOG_ERROR("Error at starting thread");
        goto free_memory;
    }
    
    return 0;

free_memory:
    free(worker_arg);
close_socketpair:
    close(sockp[0]);
    close(sockp[1]);

    return -1;
}

void signal_handler(int sign_nr)
{
	if(sign_nr == SIGINT)
	{
		LOG_MSG("Closing all the clients...");

		for(int i = 0; i < MAX_THREADS; i++)
		{
            if(clients_connected[i] != -1)
			{
                int msg = -1;
    			send_message(workers[i].socket, (const char*)&msg, sizeof(int));
    			pthread_join(workers[i].thread_handle, NULL);
            }
		}

		LOG_MSG("Closing server ...");

        close(server);

	    LOG_MSG("Done!");

        exit(0);
	}
}

int setup_server()
{
    struct sockaddr_in server_address;
    
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1)
    {
        LOG_ERROR("Eroare la creare socket");
        return -1;
    }

    int enable = 1;

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1)
    {
        LOG_ERROR("Eroare la socket option");
        goto close_server_socket;
    }

    if (bind(server_socket, (const struct sockaddr*)&server_address, sizeof(server_address)) == -1)
    {
        LOG_ERROR("Eroare la bind");
        goto close_server_socket;
    }

    if (listen(server_socket, BACK_LOG) == -1)
    {
        LOG_ERROR("Eroare la listen");
        goto close_server_socket;
    }

    return server_socket;

    close_server_socket:
        close(server_socket);
        return -1;
}

int run()
{
    if(signal(SIGINT, signal_handler) == SIG_ERR)
    {
    	LOG_ERROR("Eroare la signal");
    	return -1;
    }

    memset(clients_connected, -1, MAX_THREADS * sizeof(int));

    int server_socket = setup_server();

    if(server_socket == -1)
    {
        LOG_ERROR("Error at setup_server");
        return -1;
    }

    printf("Server running on port %d\n", SERVER_PORT);

    int client_socket;

    while(1)
    {
    	client_socket = accept(server_socket, NULL, NULL);

        if(client_socket == -1)
        {
            LOG_ERROR("Error at accepting client");
        }
        else
    	   dispatch_client(workers, client_socket);
    }

    return 0;
}
