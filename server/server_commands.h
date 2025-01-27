#ifndef SERVER_COMMANDS_H
#define SERVER_COMMANDS_H

#include "types.h"
#include <dirent.h>
#include <sys/file.h>

#define DATA_TRANSFER_CHUNK 1024 * 32

extern struct mapped_file accounts;
pthread_mutex_t accounts_mutex;

int get_hint_new(struct entry* client);
enum client_result get_hint(struct entry* client, enum client_events event);

int send_file_new(struct entry* client);
enum client_result send_file(struct entry* client, enum client_events event);
void send_file_free(struct entry* client);

int get_command_new(struct entry* client);
enum client_result get_command(struct entry* client, enum client_events event);

int login_new(struct entry* client);
enum client_result login(struct entry* client, enum client_events event);

int create_account_new(struct entry* client);
enum client_result create_account(struct entry* client, enum client_events event);

int get_file_new(struct entry* client);
enum client_result get_file(struct entry* client, enum client_events event);
void get_file_free(struct entry* client);

int ls_new(struct entry* client);
enum client_result ls(struct entry* client, enum client_events event);
void ls_free(struct entry* client);

enum client_result cd(struct entry* client, enum client_events event);
int cd_new(struct entry* client);

#endif