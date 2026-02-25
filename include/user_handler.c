#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "logger.h"
#include "file_handler.h"

#define USER_FILE "data/users.dat"

// Helper to get next ID
int get_next_user_id() {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) return 1; // File doesn't exist, start at 1

    User u;
    int max_id = 0;
    
    // Read sequentially to find max ID (Locking not strictly needed for just reading size)
    while(read(fd, &u, sizeof(User)) > 0) {
        if (u.id > max_id) max_id = u.id;
    }
    close(fd);
    return max_id + 1;
}

int register_user(char *username, char *password, int role, int initial_balance) {
    int fd = open(USER_FILE, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("Open Error");
        return -1;
    }

    // Lock the ENTIRE file for writing to ensure unique ID and append
    // (We use lock_record from Phase 1, but offset 0 and size 0 means "Whole File" in some systems, 
    // but here we will just lock the end of file roughly)
    // Actually, for append, used a write lock on the whole file for safety.
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock whole file
    fcntl(fd, F_SETLKW, &lock);

    // Check if username exists
    User u;
    while(read(fd, &u, sizeof(User)) > 0) {
        if (strcmp(u.username, username) == 0) {
            // Unlock and close
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLKW, &lock);
            close(fd);
            return -2; // Username taken
        }
    }

    // Create new user
    User new_user;
    new_user.id = get_next_user_id(); // This helper re-opens file, but we are holding lock so it's safe-ish 
    // (Actually simpler: just count the read loop above to find max_id, but for now let's hardcode next logic)
    // Refined logic: We are at EOF now.
    new_user.id = (lseek(fd, 0, SEEK_END) / sizeof(User)) + 1;
    strcpy(new_user.username, username);
    strcpy(new_user.password, password);
    new_user.role = role;
    new_user.balance = initial_balance;

    write(fd, &new_user, sizeof(User));

    char log_buffer[256];
    sprintf(log_buffer, "New User Registered: ID %d Name %s", new_user.id, username);
    write_log(log_buffer);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    return new_user.id;
}

int get_user_balance(int user_id) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) return -1;

    off_t offset = (user_id - 1) * sizeof(User);
    
    if (lock_record(fd, F_RDLCK, offset, sizeof(User)) == -1) {
        close(fd); return -1;
    }

    User u;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &u, sizeof(User)) <= 0) {
        unlock_record(fd, offset, sizeof(User));
        close(fd); 
        return -1;
    }

    unlock_record(fd, offset, sizeof(User));
    close(fd);
    return u.balance;
}

int authenticate_user(char *username, char *password) {
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) return -1;

    // Read Lock Whole File (Shared)
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);

    User u;
    int found_id = -1;
    while(read(fd, &u, sizeof(User)) > 0) {
        if (strcmp(u.username, username) == 0 && strcmp(u.password, password) == 0) {
            found_id = u.id;
            break;
        }
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    return found_id;
}

int transfer_funds(int from_user_id, int to_user_id, int amount) {
    int fd = open(USER_FILE, O_RDWR);
    if (fd == -1) return -1;

    // DEADLOCK PREVENTION: Always lock smaller ID first
    int first_id = (from_user_id < to_user_id) ? from_user_id : to_user_id;
    int second_id = (from_user_id < to_user_id) ? to_user_id : from_user_id;

    off_t offset1 = (first_id - 1) * sizeof(User);
    off_t offset2 = (second_id - 1) * sizeof(User);

    // 1. Lock First User
    if (lock_record(fd, F_WRLCK, offset1, sizeof(User)) == -1) {
        close(fd); return -1;
    }
    
    // 2. Lock Second User
    if (lock_record(fd, F_WRLCK, offset2, sizeof(User)) == -1) {
        unlock_record(fd, offset1, sizeof(User)); // Rollback
        close(fd); return -1;
    }

    // 3. Read Both Users
    User u1, u2;
    lseek(fd, offset1, SEEK_SET); read(fd, &u1, sizeof(User));
    lseek(fd, offset2, SEEK_SET); read(fd, &u2, sizeof(User));

    // 4. Identify Payer and Payee (since we swapped IDs for locking)
    User *payer = (u1.id == from_user_id) ? &u1 : &u2;
    User *payee = (u1.id == to_user_id) ? &u1 : &u2;

    char log_msg[200];
    sprintf(log_msg, "Transaction in progress: User %d (%s) transferring $%d to User %d (%s)", 
            from_user_id, payer->username, amount, to_user_id, payee->username);
    write_log(log_msg);

    // 5. Check Balance
    if (payer->balance < amount) {
        // Insufficient funds
        unlock_record(fd, offset2, sizeof(User));
        unlock_record(fd, offset1, sizeof(User));
        close(fd);
        sprintf(log_msg, "Transaction failed: User %d (%s) has insufficient funds.", 
                from_user_id, payer->username);
        write_log(log_msg);
        return -2;
    }

    // 6. Perform Transfer
    payer->balance -= amount;
    payee->balance += amount;

    // 7. Write Back
    lseek(fd, offset1, SEEK_SET); write(fd, &u1, sizeof(User));
    lseek(fd, offset2, SEEK_SET); write(fd, &u2, sizeof(User));

    // 8. Unlock Both
    unlock_record(fd, offset2, sizeof(User));
    unlock_record(fd, offset1, sizeof(User));
    
    close(fd);
    sprintf(log_msg, "Transaction successful: User %d (%s) transferred $%d to User %d (%s)", 
            from_user_id, payer->username, amount, to_user_id, payee->username);
    write_log(log_msg);
    return 1;
}

void get_username(int user_id, char *buffer) {
    strcpy(buffer, "Unknown"); // Default fallback
    if (user_id <= 0) return;
    
    int fd = open(USER_FILE, O_RDONLY);
    if (fd == -1) return;

    User u;
    // Jump directly to the user's record
    lseek(fd, (user_id - 1) * sizeof(User), SEEK_SET);
    if (read(fd, &u, sizeof(User)) > 0) {
        strcpy(buffer, u.username); // Copy name to buffer
    }
    close(fd);
}

int update_balance(int user_id, int amount_change) {
    int fd = open(USER_FILE, O_RDWR);
    if (fd == -1) return -1;
    
    off_t offset = (user_id - 1) * sizeof(User);
    if (lock_record(fd, F_WRLCK, offset, sizeof(User)) == -1) {
        close(fd); return -1;
    }
    
    User u;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &u, sizeof(User)) <= 0) {
        unlock_record(fd, offset, sizeof(User));
        close(fd); return -1;
    }
    
    // If deducting, check if balance is sufficient
    if (amount_change < 0 && u.balance < -amount_change) {
        unlock_record(fd, offset, sizeof(User));
        close(fd); return -2; // Insufficient Funds
    }
    
    u.balance += amount_change;
    
    lseek(fd, offset, SEEK_SET);
    write(fd, &u, sizeof(User));
    
    unlock_record(fd, offset, sizeof(User));
    close(fd);
    return 1;
}