#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "include/common.h"

void clear_input() { while (getchar() != '\n'); }

int recv_all(int sock, void *buffer, size_t length) {
    size_t total_received = 0;
    char *ptr = (char *)buffer;
    while (total_received < length) {
        ssize_t received = recv(sock, ptr + total_received, length - total_received, 0);
        if (received <= 0) return received;
        total_received += received;
    }
    return total_received;
}

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
            int initial_balance = 0;
            
            printf("Enter Username: "); scanf("%s", req.username);
            printf("Enter Password: "); scanf("%s", req.password);
            printf("Enter Initial Balance: "); scanf("%d", &initial_balance);
            
            // Send the balance in the payload
            sprintf(req.payload, "%d", initial_balance);
            
            send(sock, &req, sizeof(Request), 0);
            recv_all(sock, &res, sizeof(Response));
            printf("Server: %s\n", res.message);
        }
        else if (choice == 2) {
            req.operation = OP_LOGIN;
            printf("Enter Username: "); scanf("%s", req.username);
            printf("Enter Password: "); scanf("%s", req.password);
            if (send(sock, &req, sizeof(Request), 0) < 0) {
                printf("Error: Send failed.\n");
                break;
            }
            
            int bytes_received = recv(sock, &res, sizeof(Response), 0);
            if (bytes_received <= 0) {
                printf("Error: Connection lost with server.\n");
                close(sock);
                return -1; // Exit or handle reconnection
            }
            printf("Server: %s\n", res.message);
            
            if (res.operation == OP_SUCCESS) {
                // --- ENTERING AUCTION MENU LOOP ---
                int logged_in = 1;
                while(logged_in) {
                    printf("\n--- AUCTION MENU ---\n");
                    printf("1. List New Item (Sell)\n");
                    printf("2. View All Items (Buy)\n");
                    printf("3. Place Bid\n");
                    printf("4. Close Auction (Seller)\n");
                    printf("5. Check Balance\n");
                    printf("6. My Active Bid\n");
                    printf("7. Logout\n");
                    printf("Enter choice: ");
                    int menu_choice;
                    scanf("%d", &menu_choice);
                    clear_input();

                    if (menu_choice == 1) { // Create Item
                        req.operation = OP_CREATE_ITEM;
                        char name[50], desc[100];
                        int price, duration;
                        
                        printf("Item Name: "); scanf(" %[^\n]", name); clear_input();
                        printf("Description: "); scanf(" %[^\n]", desc); clear_input();
                        printf("Base Price: "); scanf("%d", &price); clear_input();
                        printf("Duration (in minutes): "); scanf("%d", &duration); clear_input();
                        
                        sprintf(req.payload, "%s|%s|%d|%d", name, desc, price, duration);            
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }   
                    else if (menu_choice == 2) { // View Items
                        req.operation = OP_LIST_ITEMS;
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        int count = atoi(res.message);
                        
                        printf("\nFound %d Auctions:\n", count);
                        printf("%-5s %-20s %-10s %-15s %-15s\n", "ID", "Name", "Price", "High Bidder", "Time Left");
                        printf("----------------------------------------------------------------------\n");
                        
                        Item item;
                        time_t now = time(NULL);

                        for(int i=0; i<count; i++) {
                            recv_all(sock, &item, sizeof(Item));
                            
                            char time_str[20];
                            int seconds_left = (int)difftime(item.end_time, now);

                            // Logic: If status is SOLD or Time is <= 0, it has Ended.
                            if (item.status == ITEM_SOLD || seconds_left <= 0) {
                                strcpy(time_str, "Ended");
                            } else {
                                int min = seconds_left / 60;
                                int sec = seconds_left % 60;
                                sprintf(time_str, "%dm %ds", min, sec);
                            }

                            printf("%-5d %-20s %-10d %-15d %-15s\n", 
                                   item.id, item.name, item.current_bid, item.current_winner_id, time_str);
                        }
                    }
                    else if (menu_choice == 3) {
                        req.operation = OP_BID;
                        int item_id, amount;
                        
                        printf("Enter Item ID to bid on: "); 
                        scanf("%d", &item_id);
                        printf("Enter your Bid Amount: "); 
                        scanf("%d", &amount);
                        clear_input();
                        
                        sprintf(req.payload, "%d|%d", item_id, amount);
                        send(sock, &req, sizeof(Request), 0);
                        
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 4) {
                        req.operation = OP_CLOSE_AUCTION;
                        printf("Enter Item ID to Close: ");
                        int cid;
                        scanf("%d", &cid);
                        sprintf(req.payload, "%d", cid);
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 5) {
                        req.operation = OP_VIEW_BALANCE;
                        // No payload needed, server knows ID from session
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 6) { // My Active Bids
                        req.operation = OP_MY_BIDS;
                        send(sock, &req, sizeof(Request), 0);
                        
                        recv_all(sock, &res, sizeof(Response));
                        int count = atoi(res.message);
                        
                        printf("\n--- ITEMS YOU ARE WINNING (%d) ---\n", count);
                        
                        if (count > 0) {
                            // 1. HEADER: Fixed width columns
                            printf("%-5s %-20s %-15s\n", "ID", "Name", "Current Bid");
                            printf("------------------------------------------\n");
                        } else {
                            printf("You have no active bids.\n");
                        }
                        
                        Item item;
                        for(int i=0; i<count; i++) {
                            recv_all(sock, &item, sizeof(Item));
                            
                            // 2. ROW: Matching fixed widths
                            printf("%-5d %-20s %-15d\n", 
                                   item.id, 
                                   item.name, 
                                   item.current_bid);
                        }
                    }
                    else if (menu_choice == 7) {
                        req.operation = OP_EXIT;
                        send(sock, &req, sizeof(Request), 0);
                        logged_in = 0;
                        printf("Logged out.\n");
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