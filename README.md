# Real-Time Online Auction System

A concurrent, multi-threaded auction platform implemented in C, demonstrating advanced System Programming concepts including Record-Level Locking, Semaphores, and Socket Programming.

## 🚀 Key Technical Features

- **Concurrency:** Handles multiple clients simultaneously using `pthread`.
- **Record-Level Locking (`fcntl`):** \* Unlike simple file locking, this system allows User A to bid on Item #1 while User B bids on Item #2 simultaneously.
  - Implements **Readers-Writer Lock** logic (Shared locks for viewing, Exclusive locks for bidding).
- **Deadlock Prevention:** \* Fund transfer transactions use a strictly ordered locking protocol (always lock smaller ID first) to prevent circular wait conditions.
- **Persistence:** All data is stored in binary files (`users.dat`, `items.dat`) for efficiency.
- **Session Management:** In-memory session tracking prevents the same user from logging in twice.

## 🛠️ Architecture

- **Server:** Multi-threaded TCP server.
- **Client:** Menu-driven command-line interface.
- **Database:** Binary flat-files with offset-based random access (`lseek`).

## ⚙️ How to Compile & Run

1.  **Build the Project:**

    ```bash
    make
    ```

    _(This compiles server, client, and initializes the binary database files)_

2.  **Start the Server:**

    ```bash
    ./server
    ```

    _Runs on Port 8888 by default._

3.  **Start a Client (Open multiple terminals):**
    ```bash
    ./client
    ```

## 🧪 Suggested Test Scenarios

1.  **The Bidding War:** Open two terminals. User A bids on an item. User B tries to bid a lower amount (rejected) and then a higher amount (accepted).
2.  **Concurrency Check:** Have User A sit on the "Place Bid" screen (which holds a lock if implemented with wait). Have User B try to bid on the _same_ item.
3.  **Transaction:** Seller closes an auction. Verify Winner's balance decreases and Seller's balance increases.

## 📂 File Structure

- `server.c` / `client.c`: Entry points.
- `include/file_handler.c`: Low-level wrapper for `fcntl` locking.
- `include/item_handler.c`: Logic for bidding and item creation.
- `include/user_handler.c`: Logic for auth and funds transfer.
- `include/logger.c`: Thread-safe audit logging.
