#ifndef COMMON_H
#define COMMON_H

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Operation Codes (Client -> Server)
#define OP_LOGIN 1
#define OP_REGISTER 2
#define OP_EXIT 3
#define OP_SUCCESS 100
#define OP_ERROR 101

// User Roles
#define ROLE_ADMIN 1
#define ROLE_USER 2

// Data Structures
typedef struct {
    int id;
    char username[50];
    char password[50];
    int role;         // ROLE_ADMIN or ROLE_USER
} User;

// Protocol Message
typedef struct {
    int operation;    // OP_LOGIN, etc.
    int session_id;   // -1 if not logged in
    char username[50];
    char password[50]; // text plain
    char payload[BUFFER_SIZE]; // Generic message/data
} Request;

typedef struct {
    int operation;
    char message[BUFFER_SIZE];
    int session_id; // Assigned by server on login
} Response;

#endif