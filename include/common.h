#ifndef COMMON_H
#define COMMON_H

#define PORT 8080
#define BUFFER_SIZE 1024

// User Roles
#define ROLE_ADMIN 1
#define ROLE_USER 2  // Can be both buyer and seller

// Item Status
#define ITEM_ACTIVE 1
#define ITEM_SOLD 2
#define ITEM_REJECTED 3

typedef struct {
    int id;                 // Unique User ID
    char username[50];
    char password[50];
    int type;               // ROLE_ADMIN or ROLE_USER
    int is_logged_in;       // Simple session tracking
} User;

typedef struct {
    int id;                 // Unique Item ID
    char name[50];
    char description[100];
    int seller_id;
    int current_winner_id;  // ID of the highest bidder
    int base_price;
    int current_bid;
    char end_date[20];
    int status;             // ITEM_ACTIVE, SOLD, etc.
} Item;

// Structure to send messages between client/server
typedef struct {
    int type;               // 1=Login, 2=List Item, 3=Bid
    char data[BUFFER_SIZE];
    int session_id;         // To track who sent it
} Request;

#endif