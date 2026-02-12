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
                    printf("1. List New Item (Sell)\n");
                    printf("2. View All Items (Buy)\n");
                    printf("3. Place Bid\n");
                    printf("4. Close Auction (Seller)\n");
                    printf("5. Check Balance\n");
                    printf("6. Logout\n");
                    printf("Enter choice: ");
                    int menu_choice;
                    scanf("%d", &menu_choice);
                    clear_input();

                    if (menu_choice == 1) {
                        req.operation = OP_CREATE_ITEM;
                        char name[50], desc[100], date[20];
                        int price;
                        
                        printf("Item Name: "); scanf("%[^\n]", name); clear_input();
                        printf("Description: "); scanf("%[^\n]", desc); clear_input();
                        printf("Base Price: "); scanf("%d", &price); clear_input();
                        printf("End Date (YYYY-MM-DD): "); scanf("%s", date); clear_input();
                        
                        // Pack into payload
                        sprintf(req.payload, "%s|%s|%d|%s", name, desc, price, date);
                        send(sock, &req, sizeof(Request), 0);
                        recv(sock, &res, sizeof(Response), 0);
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 2) {
                        req.operation = OP_LIST_ITEMS;
                        send(sock, &req, sizeof(Request), 0);
                        
                        // Read Count
                        recv(sock, &res, sizeof(Response), 0);
                        int count = atoi(res.message);
                        printf("\nFound %d Active Auctions:\n", count);
                        printf("ID\tName\t\tPrice\tHighest Bidder\n");
                        printf("----------------------------------------------------\n");
                        
                        Item item;
                        for(int i=0; i<count; i++) {
                            recv(sock, &item, sizeof(Item), 0);
                            printf("%d\t%s\t\t%d\t%d\n", item.id, item.name, item.current_bid, item.current_winner_id);
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
                        
                        recv(sock, &res, sizeof(Response), 0);
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 4) {
                        req.operation = OP_CLOSE_AUCTION;
                        printf("Enter Item ID to Close: ");
                        int cid;
                        scanf("%d", &cid);
                        sprintf(req.payload, "%d", cid);
                        send(sock, &req, sizeof(Request), 0);
                        recv(sock, &res, sizeof(Response), 0);
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 5) {
                        req.operation = OP_VIEW_BALANCE;
                        // No payload needed, server knows ID from session
                        send(sock, &req, sizeof(Request), 0);
                        recv(sock, &res, sizeof(Response), 0);
                        printf("Server: %s\n", res.message);
                    }
                    else if (menu_choice == 6) {
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