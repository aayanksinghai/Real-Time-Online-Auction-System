#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "include/common.h"
#include "include/file_handler.h" // You need to add prototypes for user_handler here or create a user_handler.h
#include "include/session.h"

int register_user(char *username, char *password, int role);
int authenticate_user(char *username, char *password);

void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);
    
    Request req;
    Response res;
    int my_user_id = -1;

    while(recv(sock, &req, sizeof(Request), 0) > 0) {
        memset(&res, 0, sizeof(Response));
        
        switch(req.operation) {
            case OP_REGISTER:
                printf("Register request: %s\n", req.username);
                
                int reg_status = register_user(req.username, req.password, ROLE_USER);
                if (reg_status > 0) {
                    res.operation = OP_SUCCESS;
                    strcpy(res.message, "Registration Successful! Please Login.");
                } else if (reg_status == -2) {
                    res.operation = OP_ERROR;
                    strcpy(res.message, "Username already exists.");
                } else {
                    res.operation = OP_ERROR;
                    strcpy(res.message, "Server Error.");
                }
                break;

            case OP_LOGIN:
                printf("Login request: %s\n", req.username);
                int user_id = authenticate_user(req.username, req.password);
                if (user_id > 0) {
                    int session_status = create_session(user_id);
                    if (session_status >= 0) {
                        my_user_id = user_id;
                        res.operation = OP_SUCCESS;
                        res.session_id = session_status;
                        sprintf(res.message, "Welcome User ID %d", user_id);
                    } else {
                        res.operation = OP_ERROR;
                        strcpy(res.message, "User already logged in.");
                    }
                } else {
                    res.operation = OP_ERROR;
                    strcpy(res.message, "Invalid Credentials.");
                }
                break;

            case OP_EXIT:
                if (my_user_id != -1) remove_session(my_user_id);
                close(sock);
                return NULL;
        }
        send(sock, &res, sizeof(Response), 0);
    }
    
    if (my_user_id != -1) remove_session(my_user_id);
    close(sock);
    return NULL;
}

int main() {
    init_sessions(); // Initialize the session array
    
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
    
    printf("Auction Server running on port %d\n", PORT);
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
        pthread_t thread_id;
        new_sock = malloc(sizeof(int)); *new_sock = new_socket;
        pthread_create(&thread_id, NULL, client_handler, (void*)new_sock);
    }
    return 0;
}