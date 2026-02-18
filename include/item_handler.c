#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"
#include "file_handler.h"
#include "logger.h"

#define ITEM_FILE "data/items.dat"

extern int transfer_funds(int from_user_id, int to_user_id, int amount);

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
    new_item.current_winner_id = -1; // No bidder yet (no bids have been placed yet)
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

// Returns: 
// 1 = Success
// -1 = File Error
// -2 = Item not found
// -3 = Bid too low (someone else bid higher while you were thinking)
// -4 = Auction ended
int place_bid(int item_id, int user_id, int bid_amount) {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return -1;

    // 1. Calculate Offset for Record Locking
    // Items are 1-indexed in our logic, so we subtract 1 to get 0-indexed offset
    // However, in create_item we did: id = (size / sizeof) + 1. 
    // So Item ID 1 is at offset 0. Item ID 2 is at offset sizeof(Item).
    off_t offset = (item_id - 1) * sizeof(Item);

    // 2. APPLY WRITE LOCK (EXCLUSIVE) ON THIS SPECIFIC RECORD
    // This allows other users to bid on Item #2 while we lock Item #1
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(Item);
    
    // Blocking call: wait if someone else is currently bidding on THIS item
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return -1;
    }

    // 3. Read the Item (Critical Section)
    Item item;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &item, sizeof(Item)) <= 0) {
        // Unlock and fail
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
        close(fd);
        return -2; // Item ID invalid
    }

    // 4. Validate Logic (The "Business Rules")
    if (item.status != ITEM_ACTIVE) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
        close(fd);
        return -4; // Auction ended
    }

    if (bid_amount <= item.current_bid) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
        close(fd);
        return -3; // Someone outbid you or bid is too low
    }

    item.current_bid = bid_amount;
    item.current_winner_id = user_id;
    
    // 6. Write Back
    lseek(fd, offset, SEEK_SET);
    if (write(fd, &item, sizeof(Item)) != sizeof(Item)) {
        // Critical Error: Write failed (Disk full? Corruption?)
        perror("Write failed in place_bid");
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
        close(fd);
        return -1;
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    
    close(fd);
    char log_msg[100];
    sprintf(log_msg, "User %d placed bid %d on Item %d", user_id, bid_amount, item_id);
    write_log(log_msg);
    return 1;
}

int close_auction(int item_id, int seller_id) {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return -1;

    off_t offset = (item_id - 1) * sizeof(Item);

    // 1. Lock Item
    if (lock_record(fd, F_WRLCK, offset, sizeof(Item)) == -1) {
        close(fd); return -1;
    }

    Item item;
    lseek(fd, offset, SEEK_SET);
    read(fd, &item, sizeof(Item));

    // 2. Validate
    if (item.seller_id != seller_id) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -2; // Not your item
    }
    if (item.status != ITEM_ACTIVE) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -3; // Already closed
    }
    if (item.current_winner_id == -1) {
        // No bids? Just close it without transfer
        item.status = ITEM_SOLD; // Or ITEM_REJECTED
        lseek(fd, offset, SEEK_SET); write(fd, &item, sizeof(Item));
        unlock_record(fd, offset, sizeof(Item)); close(fd);
        return 0; // Closed with no winner
    }

    // 3. Perform Transaction (Winner -> Seller)
    int trans_status = transfer_funds(item.current_winner_id, item.seller_id, item.current_bid);
    
    if (trans_status == 1) {
        // Success! Mark item sold
        item.status = ITEM_SOLD;
        lseek(fd, offset, SEEK_SET);
        write(fd, &item, sizeof(Item));
    }

    // 4. Unlock Item
    unlock_record(fd, offset, sizeof(Item));
    close(fd);

    char log_msg[100];
    sprintf(log_msg, "Auction closed for Item %d. Winner: %d, Amount: %d", item_id, item.current_winner_id, item.current_bid);
    write_log(log_msg);
    
    return trans_status; // 1 = Success, -2 = Low Balance
}

// Returns number of items found where user is the highest bidder
int get_my_bids(int user_id, Item *buffer, int max_items) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

    // Shared Lock
    if (lock_record(fd, F_RDLCK, 0, 0) == -1) { // 0,0 = Lock Whole File (simpler for scanning)
        close(fd); return 0;
    }

    Item item;
    int count = 0;
    while(read(fd, &item, sizeof(Item)) > 0 && count < max_items) {
        if (item.current_winner_id == user_id && item.status == ITEM_ACTIVE) {
            buffer[count++] = item;
        }
    }

    // Unlock
    unlock_record(fd, 0, 0);
    close(fd);
    return count;
}