#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "include/common.h"

void clear_input() { while (getchar() != '\n'); }

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        return -1;

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
        return -1;

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        return -1;

    int choice;
    Request req;
    Response res;

    while(1) {
        printf("\n1. Register\n2. Login\n3. Exit\nEnter choice: ");
        scanf("%d", &choice);
        clear_input();

        memset(&req, 0, sizeof(Request));

        if (choice == 1) {
            req.operation = OP_REGISTER;
            printf("Enter Username: "); scanf("%s", req.username);
            printf("Enter Password: "); scanf("%s", req.password);
            send(sock, &req, sizeof(Request), 0);
            recv(sock, &res, sizeof(Response), 0);
            printf("Server: %s\n", res.message);
        }
        else if (choice == 2) {
            req.operation = OP_LOGIN;
            printf("Enter Username: "); scanf("%s", req.username);
            printf("Enter Password: "); scanf("%s", req.password);
            send(sock, &req, sizeof(Request), 0);
            
            recv(sock, &res, sizeof(Response), 0);
            printf("Server: %s\n", res.message);
            
            if (res.operation == OP_SUCCESS) {
                // --- ENTERING AUCTION MENU LOOP ---
                int logged_in = 1;
                while(logged_in) {
                    printf("\n--- AUCTION MENU ---\n");
                    printf("1. List Items (Placeholder)\n");
                    printf("2. Logout\n");
                    printf("Enter choice: ");
                    
                    int menu_choice;
                    scanf("%d", &menu_choice);
                    clear_input();

                    if (menu_choice == 2) {
                        // Send Exit Request to Server to clear session
                        req.operation = OP_EXIT;
                        send(sock, &req, sizeof(Request), 0);
                        logged_in = 0; // Break loop to go back to main menu
                        printf("Logged out successfully.\n");
                    } else {
                        printf("Feature coming in Phase 3!\n");
                    }
                }
            }
        }
        else if (choice == 3) {
            req.operation = OP_EXIT;
            send(sock, &req, sizeof(Request), 0);
            break;
        }
    }
    close(sock);
    return 0;
}