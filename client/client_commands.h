#ifndef CLIENT_COMMANDS_H
#define CLIENT_COMMANDS_H
#include "types.h"
int create_account(SOCKET socket, int index);
int login(SOCKET socket, int index);
int get_file(SOCKET socket, int index);
int send_file(SOCKET socket, int index);
int ls(SOCKET socket, int index);
int cd(SOCKET socket, int index);
#endif