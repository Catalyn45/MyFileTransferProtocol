CC = gcc

CFLAGS_WARNINGS = \
	-pedantic -Wall -Wextra -Werror -Winline

LIBS = -lpthread

ftp_server: main.o server.o
	$(CC) main.o server.o -o ftp_server $(LIBS)

ftp_server_debug: main_d.o server_d.o
	$(CC) main_d.o server_d.o -o ftp_server_debug $(LIBS)

main.o: main.c
	$(CC) -c $(CFLAGS_WARNINGS) -Os main.c -o main.o
server.o: server.c server.h
	$(CC) -c $(CFLAGS_WARNINGS) -Os server.c -o server.o

main_d.o: main.c
	$(CC) -c -DDEBUG $(CFLAGS_WARNINGS) -g main.c -o main_d.o
server_d.o: server.c server.h
	$(CC) -c -DDEBUG $(CFLAGS_WARNINGS) -g server.c -o server_d.o
clean:
	find . -name "*.o" -type f -delete
	find . -name "ftp_server" -type f -delete

all: ftp_server

debug: ftp_server_debug
