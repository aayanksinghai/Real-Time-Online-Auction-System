#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "file_handler.h"

#define ITEM_FILE "data/items.dat"

int get_next_item_id(int fd) {
    long size = lseek(fd, 0, SEEK_END);
    if (size == 0) return 1;
    return (size / sizeof(Item)) + 1;
}

int create_item(char *name, char *desc, int base_price, char *date, int seller_id) {
    int fd = open(ITEM_FILE, O_RDWR | O_CREAT, 0666);
    if (fd == -1) return -1;

    // LOCK the file (Exclusive) to ensure we get a unique ID and append safely
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // Lock whole file for appending
    fcntl(fd, F_SETLKW, &lock);

    // Prepare the Item
    Item new_item;
    new_item.id = get_next_item_id(fd);
    strcpy(new_item.name, name);
    strcpy(new_item.description, desc);
    new_item.base_price = base_price;
    new_item.current_bid = base_price; // Initial bid is base price
    strcpy(new_item.end_date, date);
    new_item.seller_id = seller_id;
    new_item.current_winner_id = -1; // No bidder yet
    new_item.status = ITEM_ACTIVE;

    // Write to file
    lseek(fd, 0, SEEK_END);
    write(fd, &new_item, sizeof(Item));

    // Unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    
    return new_item.id;
}

// Reads all items into a buffer to send to client
// Returns number of items found
int get_all_items(Item *buffer, int max_items) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

    // Shared Lock (Read Lock) 
    // Allows multiple people to view items at the same time
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock);

    int count = 0;
    while(read(fd, &buffer[count], sizeof(Item)) > 0 && count < max_items) {
        count++;
    }

    // Unlock
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    
    return count;
}