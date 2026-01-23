#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "include/common.h"

void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);
    
    char buffer[BUFFER_SIZE];
    send(sock, "Welcome to the Auction System!\n", 29, 0);
    
    // Simple echo for now
    memset(buffer, 0, BUFFER_SIZE);
    while(recv(sock, buffer, BUFFER_SIZE, 0) > 0) {
        printf("Client said: %s\n", buffer);
        send(sock, "Acknowledged\n", 13, 0);
        memset(buffer, 0, BUFFER_SIZE);
    }
    
    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket, *new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Auction Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread_id;
        new_sock = malloc(sizeof(int));
        *new_sock = new_socket;
        
        if (pthread_create(&thread_id, NULL, client_handler, (void*)new_sock) < 0) {
            perror("Thread creation failed");
            return 1;
        }
    }
    return 0;
}