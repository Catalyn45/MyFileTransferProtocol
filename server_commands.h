#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H
#include "types.h"
#include <dirent.h>

#define DATA_TRANSFER_CHUNK 1024 * 32

int send_file_new(struct entry* client);
enum client_result send_file(struct entry* client, enum client_events event);
void send_file_free(struct entry* client);

int get_command_new(struct entry* client);
enum client_result get_command(struct entry* client, enum client_events event);
void get_command_free(struct entry* client);

int login_new(struct entry* client);
enum client_result login(struct entry* client, enum client_events event);
void login_free(struct entry* client);


#endif