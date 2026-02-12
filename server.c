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
int create_item(char *name, char *desc, int base_price, char *date, int seller_id);
int get_all_items(Item *buffer, int max_items);

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


            case OP_CREATE_ITEM:
                printf("User %d listing item: %s\n", my_user_id, req.payload); 
                // Client sends "Name|Desc|Price|Date" string in payload
                
                char i_name[50], i_desc[100], i_date[20];
                int i_price;
                sscanf(req.payload, "%[^|]|%[^|]|%d|%s", i_name, i_desc, &i_price, i_date);
                
                int item_id = create_item(i_name, i_desc, i_price, i_date, my_user_id);
                
                if (item_id > 0) {
                    res.operation = OP_SUCCESS;
                    sprintf(res.message, "Item Listed Successfully! ID: %d", item_id);
                } else {
                    res.operation = OP_ERROR;
                    strcpy(res.message, "Failed to list item.");
                }
                break;

            case OP_LIST_ITEMS:
                // We need to send a list. The Response struct only has a small message buffer.
                // We will send a header first, then the items one by one.
                Item items[50];
                int count = get_all_items(items, 50);
                
                res.operation = OP_SUCCESS;
                sprintf(res.message, "%d", count); // Send count first
                send(sock, &res, sizeof(Response), 0);
                
                // Send actual items
                for(int i=0; i<count; i++) {
                    send(sock, &items[i], sizeof(Item), 0);
                }
                continue; // Skip the default send at bottom since we already sent response

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