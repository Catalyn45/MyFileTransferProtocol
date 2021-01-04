CC = gcc

CFLAGS_WARNINGS = \
	-pedantic -Wall -Wextra -Werror -Winline

client: main.o client_commands.o types.h
	$(CC) main.o client_commands.o -o client $(LIBS)

client_debug: main_d.o client_commands_d.o types.h
	$(CC) main_d.o client_commands_d.o -o client_debug $(LIBS)

main.o: main.c
	$(CC) -c $(CFLAGS_WARNINGS) -Os main.c -o main.o

main_d.o: main.c
	$(CC) -c -DDEBUG $(CFLAGS_WARNINGS) -g main.c -o main_d.o

client_commands.o: client_commands.c client_commands.h
	$(CC) -c $(CFLAGS_WARNINGS) -Os client_commands.c -o client_commands.o

client_commands_d.o: client_commands.c client_commands.h
	$(CC) -c -DDEBUG $(CFLAGS_WARNINGS) -g client_commands.c -o client_commands_d.o

clean:
	find . -name "*.o" -type f -delete
	find . -name "client" -type f -delete
	find . -name "client_debug" -type f -delete

all: client

debug: client_debug
