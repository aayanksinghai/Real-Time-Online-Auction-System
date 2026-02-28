#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"
#include "file_handler.h"
#include "logger.h"

#define ITEM_FILE "data/items.dat"

extern int update_balance(int user_id, int amount_change);
extern int get_user_cooldown(int user_id);

int get_next_item_id(int fd) {
    long size = lseek(fd, 0, SEEK_END);
    if (size == 0) return 1;
    return (size / sizeof(Item)) + 1;
}

// UPDATED: Accepts int duration_minutes
int create_item(char *name, char *desc, int base_price, int duration_minutes, int seller_id) {
    int fd = open(ITEM_FILE, O_RDWR | O_CREAT, 0666);
    if (fd == -1) return -1;

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; 
    fcntl(fd, F_SETLKW, &lock);

    Item new_item;
    new_item.id = get_next_item_id(fd);
    strcpy(new_item.name, name);
    strcpy(new_item.description, desc);
    new_item.base_price = base_price;
    new_item.current_bid = base_price;
    
    // CALCULATE EXPIRATION TIME
    new_item.end_time = time(NULL) + (duration_minutes * 60);
    
    new_item.seller_id = seller_id;
    new_item.current_winner_id = -1;
    new_item.status = ITEM_ACTIVE;

    lseek(fd, 0, SEEK_END);
    write(fd, &new_item, sizeof(Item));

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);

    char seller_name[50];
    get_username(seller_id, seller_name); // Use the helper
    
    char log_msg[150];
    sprintf(log_msg, "Seller %d (%s) listed a new item: %s (ID: %d)", 
            seller_id, seller_name, name, new_item.id);
    write_log(log_msg);
    
    return new_item.id;
}

int get_all_items(Item *buffer, int max_items) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

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

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);
    return count;
}

int place_bid(int item_id, int user_id, int bid_amount) {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return -1;

    off_t offset = (item_id - 1) * sizeof(Item);

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = sizeof(Item);
    
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return -1;
    }

    Item item;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &item, sizeof(Item)) <= 0) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -2; 
    }

    if (item.seller_id == user_id) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -5; // Cannot bid on your own item
    }

    if (item.status != ITEM_ACTIVE) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -4; 
    }
    
    // ADDED: Check if time expired while placing bid
    if (time(NULL) >= item.end_time) {
         lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
         return -4; 
    }

    if (bid_amount <= item.current_bid) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -3; 
    }

    // --- COOLDOWN CHECK ---
    if (get_user_cooldown(user_id) > 0) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -7; // Code -7: Cooldown Active
    }

    // --- ESCROW: Block (deduct) funds from the new bidder ---
    // Passing negative bid_amount to deduct it
    if (update_balance(user_id, -bid_amount) == -2) {
        lock.l_type = F_UNLCK; 
        fcntl(fd, F_SETLKW, &lock); 
        close(fd);
        return -6; // Code -6 means Insufficient Funds
    }

    // --- ESCROW: Refund the previous bidder ---
    // If someone else had the high bid, give them their blocked money back
    if (item.current_winner_id != -1) {
        update_balance(item.current_winner_id, item.current_bid); 
    }

    item.current_bid = bid_amount;
    item.current_winner_id = user_id;
    
    lseek(fd, offset, SEEK_SET);
    if (write(fd, &item, sizeof(Item)) != sizeof(Item)) {
        lock.l_type = F_UNLCK; fcntl(fd, F_SETLKW, &lock); close(fd);
        return -1;
    }

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLKW, &lock);
    close(fd);

    char bidder_name[50];
    get_username(user_id, bidder_name); // Use the helper
    
    char log_msg[200];
    sprintf(log_msg, "User %d (%s) placed bid $%d on Item %d (%s)", 
            user_id, bidder_name, bid_amount, item_id, item.name);
    write_log(log_msg);
    return 1;
}

int close_auction(int item_id, int seller_id) {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return -1;

    if (item_id <= 0) {
        close(fd); 
        return -4;
    }

    off_t offset = (item_id - 1) * sizeof(Item);

    if (lock_record(fd, F_WRLCK, offset, sizeof(Item)) == -1) {
        close(fd); return -1;
    }

    Item item;
    lseek(fd, offset, SEEK_SET);
    
    if (read(fd, &item, sizeof(Item)) <= 0) {
        unlock_record(fd, offset, sizeof(Item)); 
        close(fd); 
        return -4; // Code -4: Item does not exist / Invalid ID
    }

    if (item.seller_id != seller_id) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -2; // Not your item
    }
    if (item.status != ITEM_ACTIVE) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -3; // Already closed
    }
    
    // Auto-Close Logic handles no-bids, but we handle manual here:
    if (item.current_winner_id == -1) {
        item.status = ITEM_SOLD;
        item.end_time = time(NULL); // <--- FORCE TIMER TO END NOW
        
        lseek(fd, offset, SEEK_SET); write(fd, &item, sizeof(Item));
        unlock_record(fd, offset, sizeof(Item)); close(fd);
        return 0; 
    }

    int trans_status = update_balance(item.seller_id, item.current_bid);

    if (trans_status == 1) {
        item.status = ITEM_SOLD;
        item.end_time = time(NULL); // <--- FORCE TIMER TO END NOW
        
        lseek(fd, offset, SEEK_SET);
        write(fd, &item, sizeof(Item));
    }

    unlock_record(fd, offset, sizeof(Item));
    close(fd);

    char log_msg[200];
    if (item.current_winner_id == -1) {
        sprintf(log_msg, "Auction concluded manually for Item %d (%s) - No Bids.", 
                item_id, item.name);
    } else {
        char winner_name[50];
        get_username(item.current_winner_id, winner_name); // Fetch winner's name
        
        sprintf(log_msg, "Auction concluded manually for Item %d (%s). Winner: %d (%s), Final Bid: $%d", 
                item_id, item.name, item.current_winner_id, winner_name, item.current_bid);
    }
    write_log(log_msg);
    
    return trans_status; 
}

int get_my_bids(int user_id, Item *buffer, int max_items) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

    if (lock_record(fd, F_RDLCK, 0, 0) == -1) { 
        close(fd); return 0;
    }

    Item item;
    int count = 0;
    while(read(fd, &item, sizeof(Item)) > 0 && count < max_items) {
        if (item.current_winner_id == user_id && item.status == ITEM_ACTIVE) {
            buffer[count++] = item;
        }
    }

    unlock_record(fd, 0, 0);
    close(fd);
    return count;
}

// Background Monitor Logic
void check_expired_items() {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return;

    Item item;
    off_t offset = 0;
    time_t now = time(NULL);

    while (pread(fd, &item, sizeof(Item), offset) > 0) {
        if (item.status == ITEM_ACTIVE && item.end_time <= now) {
            
            if (lock_record(fd, F_WRLCK, offset, sizeof(Item)) == -1) {
                offset += sizeof(Item); continue;
            }

            pread(fd, &item, sizeof(Item), offset);
            
            if (item.status == ITEM_ACTIVE) {
                if (item.current_winner_id != -1) {
                    update_balance(item.seller_id, item.current_bid);
                    char log[100];
                    sprintf(log, "Auto-Close: Item %d sold to %d for %d", item.id, item.current_winner_id, item.current_bid);
                    write_log(log);
                } else {
                    // FIX: Use sprintf for write_log
                    char log[100];
                    sprintf(log, "Auto-Close: Item %d expired (No Bids)", item.id);
                    write_log(log);
                }

                item.status = ITEM_SOLD;
                pwrite(fd, &item, sizeof(Item), offset);
            }
            
            unlock_record(fd, offset, sizeof(Item));
        }
        offset += sizeof(Item);
    }
    close(fd);
}

// Returns completed transactions (Items Sold or Items Won)
int get_transaction_history(int user_id, Item *buffer, int max_items) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

    // Shared lock for reading
    if (lock_record(fd, F_RDLCK, 0, 0) == -1) { 
        close(fd); return 0;
    }

    Item item;
    int count = 0;
    while(read(fd, &item, sizeof(Item)) > 0 && count < max_items) {
        // Condition: Item is SOLD and the user is either the Seller or the Winner
        if (item.status == ITEM_SOLD && 
           (item.seller_id == user_id || item.current_winner_id == user_id)) {
            buffer[count++] = item;
        }
    }

    unlock_record(fd, 0, 0);
    close(fd);
    return count;
}

int is_user_seller(int user_id) {
    int fd = open(ITEM_FILE, O_RDONLY);
    if (fd == -1) return 0;

    // Shared lock for reading
    if (lock_record(fd, F_RDLCK, 0, 0) == -1) {
        close(fd); return 0;
    }

    Item item;
    int found = 0;
    while(read(fd, &item, sizeof(Item)) > 0) {
        if (item.seller_id == user_id && item.status == ITEM_ACTIVE) {
            found = 1;
            break; 
        }
    }

    unlock_record(fd, 0, 0);
    close(fd);
    return found;
}

int withdraw_bid(int item_id, int user_id) {
    int fd = open(ITEM_FILE, O_RDWR);
    if (fd == -1) return -1;
    if (item_id <= 0) { close(fd); return -4; } // Invalid ID

    off_t offset = (item_id - 1) * sizeof(Item);
    if (lock_record(fd, F_WRLCK, offset, sizeof(Item)) == -1) {
        close(fd); return -1;
    }

    Item item;
    lseek(fd, offset, SEEK_SET);
    if (read(fd, &item, sizeof(Item)) <= 0) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -4;
    }

    if (item.status != ITEM_ACTIVE) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -2; // Not active
    }
    if (item.current_winner_id != user_id) {
        unlock_record(fd, offset, sizeof(Item)); close(fd); return -3; // Not highest bidder
    }

    // Refund the user's blocked funds
    update_balance(user_id, item.current_bid);

    // Reset the item bid state
    item.current_winner_id = -1;
    item.current_bid = 0; // Drops bid to 0 so others can bid again

    lseek(fd, offset, SEEK_SET);
    write(fd, &item, sizeof(Item));

    unlock_record(fd, offset, sizeof(Item));
    close(fd);
    return 1;
}