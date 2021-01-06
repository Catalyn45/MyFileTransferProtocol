#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H

#include "types.h"
#include <dirent.h>

#define DATA_TRANSFER_CHUNK 1024 * 32

extern struct mapped_file accounts;
pthread_mutex_t accounts_mutex;

int send_file_new(struct entry* client);
enum client_result send_file(struct entry* client, enum client_events event);
void send_file_free(struct entry* client);

int get_command_new(struct entry* client);
enum client_result get_command(struct entry* client, enum client_events event);

int login_new(struct entry* client);
enum client_result login(struct entry* client, enum client_events event);

int create_account_new(struct entry* client);
enum client_result create_account(struct entry* client, enum client_events event);

int put_file_new(struct entry* client);
enum client_result put_file(struct entry* client, enum client_events event);
void put_file_free(struct entry* client);

#endif