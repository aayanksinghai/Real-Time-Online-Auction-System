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
                int my_client_id = -1;
                sscanf(res.message, "Welcome User ID %d", &my_client_id);
                // --- ENTERING AUCTION MENU LOOP ---
                int logged_in = 1;
                while(logged_in) {
                    // 1. Check if user is a seller before printing the menu
                    memset(&req, 0, sizeof(Request));
                    req.operation = OP_CHECK_SELLER;
                    send(sock, &req, sizeof(Request), 0);
                    recv_all(sock, &res, sizeof(Response));
                    int is_seller = atoi(res.message);

                    // 2. Define dynamic menu numbers
                    int opt_close  = is_seller ? 4 : -1;
                    int opt_bal    = is_seller ? 5 : 4;
                    int opt_mybids = is_seller ? 6 : 5;
                    int opt_hist   = is_seller ? 7 : 6;
                    int opt_logout = is_seller ? 8 : 7;

                    // 3. Print Dynamic Menu
                    printf("\n--- AUCTION MENU ---\n");
                    printf("1. List New Item (Sell)\n");
                    printf("2. View All Items (Buy)\n");
                    printf("3. Place Bid\n");
                    if (is_seller) {
                        printf("%d. Close Auction (Seller)\n", opt_close);
                    }
                    printf("%d. Check Balance\n", opt_bal);
                    printf("%d. My Active Bid\n", opt_mybids);
                    printf("%d. Transaction History\n", opt_hist);
                    printf("%d. Logout\n", opt_logout);
                    printf("Enter choice: ");
                    
                    int menu_choice;
                    scanf("%d", &menu_choice);
                    clear_input();
                    
                    memset(&req, 0, sizeof(Request)); // Reset req for next operations

                    if (menu_choice == 1) {
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
                    else if (menu_choice == 2) {
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
                    else if (is_seller && menu_choice == opt_close) {
                        req.operation = OP_CLOSE_AUCTION;
                        printf("Enter Item ID to Close: ");
                        int cid;
                        scanf("%d", &cid);
                        sprintf(req.payload, "%d", cid);
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == opt_bal) {
                        req.operation = OP_VIEW_BALANCE;
                        send(sock, &req, sizeof(Request), 0);
                        recv_all(sock, &res, sizeof(Response));
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == opt_mybids) {
                        req.operation = OP_MY_BIDS;
                        send(sock, &req, sizeof(Request), 0);
                        
                        recv_all(sock, &res, sizeof(Response));
                        int count = atoi(res.message);
                        
                        printf("\n--- ITEMS YOU ARE WINNING (%d) ---\n", count);
                        
                        if (count > 0) {
                            printf("%-5s %-20s %-15s\n", "ID", "Name", "Current Bid");
                            printf("------------------------------------------\n");
                        } else {
                            printf("You have no active bids.\n");
                        }
                        
                        Item item;
                        for(int i=0; i<count; i++) {
                            recv_all(sock, &item, sizeof(Item));
                            printf("%-5d %-20s %-15d\n", item.id, item.name, item.current_bid);
                        }
                    }
                    else if (menu_choice == opt_hist) {
                        req.operation = OP_TRANSACTION_HISTORY;
                        send(sock, &req, sizeof(Request), 0);
                        
                        recv_all(sock, &res, sizeof(Response));
                        int count = atoi(res.message);
                        
                        printf("\n--- TRANSACTION HISTORY ---\n");
                        if (count == 0) {
                            printf("No past transactions found.\n");
                        } else {
                            Item hist[50];
                            for(int i=0; i<count; i++) {
                                recv_all(sock, &hist[i], sizeof(Item));
                            }
                            
                            printf("\n[ ITEMS YOU SOLD ]\n");
                            printf("%-5s %-20s %-15s %-15s\n", "ID", "Name", "Final Price", "Winner ID");
                            printf("-----------------------------------------------------------\n");
                            int sold_count = 0;
                            for(int i=0; i<count; i++) {
                                if (hist[i].seller_id == my_client_id) {
                                    char winner_str[15];
                                    if (hist[i].current_winner_id == -1) strcpy(winner_str, "None");
                                    else sprintf(winner_str, "%d", hist[i].current_winner_id);
                                    
                                    printf("%-5d %-20s $%-14d %-15s\n", 
                                           hist[i].id, hist[i].name, hist[i].current_bid, winner_str);
                                    sold_count++;
                                }
                            }
                            if (sold_count == 0) printf("You haven't sold any items yet.\n");

                            printf("\n[ ITEMS YOU WON ]\n");
                            printf("%-5s %-20s %-15s %-15s\n", "ID", "Name", "Winning Bid", "Seller ID");
                            printf("-----------------------------------------------------------\n");
                            int won_count = 0;
                            for(int i=0; i<count; i++) {
                                if (hist[i].current_winner_id == my_client_id) {
                                    printf("%-5d %-20s $%-14d %-15d\n", 
                                           hist[i].id, hist[i].name, hist[i].current_bid, hist[i].seller_id);
                                    won_count++;
                                }
                            }
                            if (won_count == 0) printf("You haven't won any items yet.\n");
                        }
                    }
                    else if (menu_choice == opt_logout) {
                        req.operation = OP_EXIT;
                        send(sock, &req, sizeof(Request), 0);
                        logged_in = 0;
                        printf("Logged out.\n");
                    }
                    else {
                        printf("Invalid choice. Please try again.\n");
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