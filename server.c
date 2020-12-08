#include "server.h"

extern struct client_function commands_list[];
extern unsigned int max_cmds;

void delete_command(struct entry* client)
{
    if(client->data.current_command == NULL)
        return;

    unsigned int index = client->data.current_command->index;

    if(commands_list[index].free != NULL)
        commands_list[index].free(client->data.args);
    
    client->data.args = NULL;

    free(client->data.current_command);

    client->data.current_command = NULL;
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

enum client_result execute_command(struct entry* client)
{
    if(client == NULL)
        return CLIENT_ERROR;

    struct command* cmd = client->data.current_command;

    if(cmd == NULL)
        return CLIENT_ERROR;

    if(cmd->index >= max_cmds)
        return CLIENT_ERROR;

    return commands_list[cmd->index].work(client);
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

int get_command(struct entry* client)
{
    struct command* cmd = NULL;

    cmd = malloc(sizeof(struct command));

    if(cmd == NULL)
    {
        LOG_ERROR("Error at allocating memory");
        return -1;
    }

    client->data.current_command = cmd;

    int len = recv_message(client->data.socket, (char*)cmd, sizeof(struct command));

    return len;
}

enum client_result socket_read(struct entry* client)
{
    enum client_result result = CLIENT_WANT_READ;

    if(client->data.state == connected)
    {
        if(client->data.current_command == NULL)
        {
            int result = get_command(client);

            if(result < 0)
            {
                LOG_ERROR("Error at getting the command");
                return CLIENT_ERROR;
            }

            if(result == 0)
            {
                return CLIENT_CLOSED;
            }
        }

        result = execute_command(client);
    }

    return result;
}

int login_client(struct entry* client)
{
    int client_socket = client->data.socket;

    const char* msg = "Bine ati venit pe serverul ftp a lui Catalyn45!\n";

    int result = send_message(client_socket, msg, strlen(msg) + 1);

    if(result == -1)
    {   
        LOG_ERROR("Error at sending message");
        return CLIENT_ERROR;
    }

    client->data.state = connected;

    return CLIENT_SUCCESS;
}

enum client_result socket_write(struct entry* client)
{
    enum client_result result = CLIENT_ERROR;

    switch(client->data.state)
    {
    case login:
            result = login_client(client);
        break;
    case connected:
            result = execute_command(client);
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

            if(recv_message(main_thread_socket, (char*)&socket_to_add, sizeof(int)) <= 0)
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

            FD_SET(socket_to_add, &write_fd);   
        }
        else
        {
            struct entry* it;
            int socket;

            SLIST_FOREACH(it, &clients, entries)
            {
                socket = it->data.socket;
                enum client_result result = CLIENT_ERROR;

                if(FD_ISSET(socket, &copy_read_fd))
                {
                    result = socket_read(it);
                }
                else if(FD_ISSET(socket, &copy_write_fd))
                {
                    result = socket_write(it);
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
            }
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

	if(send_message(workers[index].socket, (const char*)&client_socket, sizeof(int)) == -1)
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
    	   dispatch_client(client_socket);
    }

    return 0;
}
