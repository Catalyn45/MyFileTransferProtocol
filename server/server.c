#include "server.h"

extern struct client_function commands_list[];
extern unsigned int max_cmds;

struct command_args
{
    int command_index;
    int buf_size;
};

void delete_error(struct entry* client)
{
    client->data.error.error_length = 0;
    client->data.error.error_size = 0;
    client->data.error.state = 0;
    free(client->data.error.buffer);
    client->data.error.buffer = NULL;
}

void delete_command(struct entry* client)
{
    client->data.current_command.cmd_state = COMMAND_NONE;

    client->data.current_command.ok_finished = 0;
    client->data.current_command.ok_size = 0;
    client->data.current_command.ok = 0;
    
    if(client->data.args == NULL)
        return;

    unsigned int index = client->data.current_command.index;

    if(index == 0)
    {
        struct command_args* args = client->data.args;
        client->data.current_command.index = args->command_index;
    }
    else
        client->data.current_command.index = 0;

    if(commands_list[index].free != NULL)
        commands_list[index].free(client);

    delete_error(client);
    
    free(client->data.args);
    client->data.args = NULL;
}

void delete_client(struct entry* it, struct slisthead* clients)
{
	SLIST_REMOVE(clients, it, entry, entries);
	close(it->data.socket);
    FD_CLR(it->data.socket, it->data.read);
    FD_CLR(it->data.socket, it->data.write);

    delete_command(it);

    free(it);
}

enum client_result execute_command(struct entry* client, enum client_events event)
{
    if(client == NULL)
        return CLIENT_ERROR;

    struct command* cmd = &client->data.current_command;

    if(cmd == NULL)
        return CLIENT_ERROR;

    if(cmd->index >= max_cmds)
        return CLIENT_ERROR;

    return commands_list[cmd->index].work(client, event);
}

enum client_result init_command(struct entry* client)
{
    if(client == NULL)
        return CLIENT_ERROR;

    struct command* cmd = &client->data.current_command;

    if(cmd == NULL)
        return CLIENT_ERROR;

    if(cmd->index >= max_cmds)
        return CLIENT_ERROR;

    if(commands_list[cmd->index].new == NULL)
        return CLIENT_WANT_READ;

    if(commands_list[cmd->index].new(client) == 0)
    {
        client->data.current_command.cmd_state = COMMAND_READY;
        return CLIENT_WANT_READ;
    }

    return CLIENT_ERROR;
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

enum client_result handle_error(struct entry* client, enum client_events event)
{
    struct error_type* cmd_error = &client->data.error;

    if(event == EVENT_READ)
    {
        if(cmd_error->state == 0)
        {
            int result = recv(client->data.socket, &cmd_error->error_length + cmd_error->error_size, sizeof(int) - cmd_error->error_size, 0);

            if(result == 0)
                return CLIENT_CLOSED;

            if(result == -1)
                return CLIENT_ERROR;

            cmd_error->error_size += result;

            if(cmd_error->error_size < sizeof(int))
                return CLIENT_AGAIN;

            if(cmd_error->error_length > MAX_ERROR_LENGTH)
                return CLIENT_ERROR;
            
            cmd_error->buffer = calloc(1, cmd_error->error_length);

            if(cmd_error == NULL)
                return CLIENT_ERROR;

            cmd_error->error_size = 0;
            cmd_error->state = 1;

            return CLIENT_AGAIN;
            
        }

        int result = recv(client->data.socket, cmd_error->buffer + cmd_error->error_size, cmd_error->error_length - cmd_error->error_size, 0);

        if(result == -1)
            return CLIENT_ERROR;

        if(result == 0)
            return CLIENT_CLOSED;

        cmd_error->error_size += result;

        if(cmd_error->error_size < cmd_error->error_length)
            return CLIENT_AGAIN;

        return CLIENT_SUCCESS;

    }
    else if(event == EVENT_WRITE)
    {
        if(cmd_error->state == 0)
        {
            struct error_info info = {-1, 0};
            info.err_length = cmd_error->error_length;

            int result = send(client->data.socket, &info + cmd_error->error_size, sizeof(struct error_info) - cmd_error->error_size, 0);

            if(result == -1)
                return CLIENT_ERROR;

            cmd_error->error_size += result;

            if(cmd_error->error_size < sizeof(struct error_info))
                return CLIENT_AGAIN;

            cmd_error->error_size = 0;
            cmd_error->state = 1;

            return CLIENT_AGAIN;
        }

        int result = send(client->data.socket, cmd_error->buffer + cmd_error->error_size, cmd_error->error_length - cmd_error->error_size, 0);

        if(result == -1)
            return CLIENT_ERROR;

        cmd_error->error_size += result;

        if(cmd_error->error_size < cmd_error->error_length)
            return CLIENT_AGAIN;

        return CLIENT_SUCCESS;
    }

    return CLIENT_ERROR;
}

enum client_result handle_event(struct entry* client, enum client_events event)
{
    switch(client->data.current_command.cmd_state)
    {
        case COMMAND_NONE:
            return init_command(client);
        case COMMAND_READY:
            return execute_command(client, event);
        case COMMAND_ERROR:
            return handle_error(client, event);
        default:
            break;
    }

    return CLIENT_ERROR;
}

void exit_thread(struct slisthead* clients, int index)
{
	struct entry* it;

	while (!SLIST_EMPTY(clients))
	{
    	it = SLIST_FIRST(clients);
        delete_client(it, clients);
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
    fd_set read_fd, copy_read_fd, write_fd, copy_write_fd;
 
    FD_ZERO(&read_fd);
    FD_ZERO(&write_fd);

    FD_SET(main_thread_socket, &read_fd);

    while(1)
    {
    	copy_read_fd = read_fd;
    	copy_write_fd = write_fd;

    	int count = select(FD_SETSIZE, &copy_read_fd, &copy_write_fd, NULL, NULL);

        if(count < 0)
        {
            LOG_ERROR("Error at select, closing thread");
            exit_thread(&clients, main_thread.index);
            pthread_detach(workers[main_thread.index].thread_handle);
            return NULL;
        }

        if FD_ISSET(main_thread_socket, &copy_read_fd)
        {
            int socket_to_add = 0;

            if(recv(main_thread_socket, (char*)&socket_to_add, sizeof(int), 0) <= 0)
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

            struct entry client;
            memset(&client, 0, sizeof(client));

            client.data.socket = socket_to_add;
            client.data.read = &read_fd;
            client.data.write = &write_fd;
            
            if(insert_client(&clients, client) == -1)
            {
                LOG_ERROR("Error at putting the client in list");
                exit_thread(&clients, main_thread.index);
                pthread_detach(workers[main_thread.index].thread_handle);
                return NULL;
            }

            FD_SET(socket_to_add, &read_fd); 
            count--;  
        }

        struct entry* it;
        int socket;

        SLIST_FOREACH(it, &clients, entries)
        {
            if(count == 0)
                break;

            socket = it->data.socket;
            enum client_result result = CLIENT_ERROR;

            if(FD_ISSET(socket, &copy_read_fd))
            {
                result = handle_event(it, EVENT_READ);
            }
            else if(FD_ISSET(socket, &copy_write_fd))
            {
                result = handle_event(it, EVENT_WRITE);
            }
            else
            {
                continue;
            }

            handle_result(it, &clients, main_thread.index, result);

            if((result == CLIENT_CLOSED || result == CLIENT_ERROR) && SLIST_EMPTY(&clients))
            {
                LOG_MSG("no clients, closing the thread");
                exit_thread(&clients, main_thread.index);
                pthread_detach(workers[main_thread.index].thread_handle);
                return NULL;
            }

            count--;
        }
    }

    return NULL;
}

void handle_result(struct entry* client, struct slisthead* clients, int index, enum client_result result)
{
    switch(result)
    {   
        case CLIENT_SUCCESS:
            delete_command(client);
        //fall through
        case CLIENT_WANT_READ:
            FD_SET(client->data.socket, client->data.read);
            FD_CLR(client->data.socket, client->data.write);
        break;

        case CLIENT_WANT_WRITE:
            FD_SET(client->data.socket, client->data.write);
            FD_CLR(client->data.socket, client->data.read);
        break;

        case CLIENT_ERROR:
            delete_client(client, clients);

            pthread_mutex_lock(&mutex);
            clients_connected[index]--;
            pthread_mutex_unlock(&mutex);

            LOG_ERROR("Error at client, client closeed");
        break;

        case CLIENT_CLOSED:
            delete_client(client, clients);

            pthread_mutex_lock(&mutex);
            clients_connected[index]--;
            pthread_mutex_unlock(&mutex);

            LOG_MSG("Client closed");
        break;
        case CLIENT_AGAIN:
        //fall through
        default:
        break;
    }
}

void dispatch_client(int client_socket)
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

			if(init_thread(&workers[i], i) == -1)
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

	if(send(workers[index].socket, (const char*)&client_socket, sizeof(int), 0) == -1)
    {
        LOG_ERROR("Can't send the socket to thread");
        close(workers[index].socket);
        close(client_socket);
        pthread_detach(workers[index].thread_handle);
        clients_connected[i] = -1;
    }
}

int init_thread(struct worker_type* worker, int index)
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
    worker_arg->index = index;

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

    pthread_mutex_destroy(&mutex);

	if(sign_nr == SIGINT)
	{
		LOG_MSG("Closing all the clients...");

		for(int i = 0; i < MAX_THREADS; i++)
		{
            if(clients_connected[i] != -1)
			{
                int msg = -1;
    			send(workers[i].socket, (const char*)&msg, sizeof(int), 0);
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
    	   dispatch_client(client_socket);
    }

    return 0;
}
