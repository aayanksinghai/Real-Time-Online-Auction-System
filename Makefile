CC = gcc
CFLAGS = -I./include -pthread

all: server client init_db

server: server.c include/file_handler.c include/user_handler.c include/session.c
	$(CC) $(CFLAGS) server.c include/file_handler.c include/user_handler.c include/session.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

# Helper to create empty binary files if they don't exist
init_db:
	mkdir -p data
	touch data/users.dat
	touch data/items.dat

clean:
	rm -f server client
	rm -rf data