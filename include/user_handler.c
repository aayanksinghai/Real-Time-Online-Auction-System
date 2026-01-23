#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
// #include "file_handler.h" // Your existing file handler

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

int register_user(char *username, char *password, int role) {
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

    write(fd, &new_user, sizeof(User));

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    return new_user.id;
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