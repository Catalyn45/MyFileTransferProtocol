#include "server_commands.h"

struct client_function commands_list[] = {
    {send_file,  send_file_free} // index 0 etc.  | free function can be NULL
};

unsigned int max_cmds = LENGTH_OF(commands_list);

enum client_result send_file(struct entry* client)
{
    struct command* cmd = client->data.current_command;

    struct transfer_info* info = client->data.args;

    char buffer[DATA_TRANSFER_CHUNK];

    if (client->data.args == NULL)
    {
        info = malloc(sizeof(struct transfer_info));

        if(info == NULL)
        {
            LOG_ERROR("Cant alloc memory");
            return CLIENT_ERROR;
        }

        int fd = open(cmd->args, O_RDONLY);

        if(fd == -1)
        {
            free(info);
            LOG_ERROR("Can't open this file");
            return CLIENT_ERROR;
        }

        info->file_descriptor = fd;

        info-> state = 0;
        client->data.args = (void*)info;

        return CLIENT_WANT_WRITE;
    }

    if(info->state == 0)
    {
        struct file_info fi;

        fi.file_type = 0;
        fi.file_size = lseek(info->file_descriptor, 0L, SEEK_END);
        lseek(info->file_descriptor, 0L, SEEK_SET);
        int result = send_message(client->data.socket, (const char*)&fi, sizeof(fi));

        if(result == -1)
        {
            LOG_ERROR("Can't send to client");
            return CLIENT_ERROR;
        }

        info->state = 1;
        return CLIENT_AGAIN;
    }
    
    if(info->state == 1)
    {
        int len = read(info->file_descriptor, buffer, DATA_TRANSFER_CHUNK);

        if(len < 0)
        {
            LOG_ERROR("Error at reading from file");
            return CLIENT_AGAIN;
        }

        if(len > 0)
        {
            if(send_message(client->data.socket, buffer, len) == -1)
            {
                LOG_ERROR("Can't send the message");
                return CLIENT_ERROR;
            }

            return CLIENT_AGAIN;
        }
        
        if(len == 0)
        {
            return CLIENT_SUCCESS;
        }
    }

    return CLIENT_ERROR;
}

void send_file_free(void* args)
{
    if(args == NULL)
        return;

    struct transfer_info* info = (struct transfer_info*)args;

    close(info->file_descriptor);

    free(info);
}