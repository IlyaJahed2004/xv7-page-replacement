# xv7 Custom Page Replacement Project: LotteryVM

##  Project Overview

This repository hosts the development of **LotteryVM**, an educational operating system project based on **xv7**.

### What is xv7?
xv7 is a fork of the famous **xv6** (MIT's educational OS). While xv6 is great, it lacks a critical feature of modern systems: complex virtual memory management. xv7 adds **Demand Paging** and **Swapping** to the kernel.

### The Core Concept: Virtual Memory
The goal is to allow processes to use more memory than the physical RAM actually available. We achieve this by using the hard disk as an extension of RAM.

** A Simple Analogy:**
Imagine you are studying at a **small desk (RAM)**, but you have a **huge bookshelf (Disk)** full of books.
1.  **Demand Paging:** You only bring a book to your desk when you specifically need to read it. You don't dump the whole shelf on the desk at once.
2.  **Swapping:** Your desk gets full. To open a new book, you must remove an old one and put it back on the shelf.
3.  **Page Replacement Policy:** How do you decide *which* book to remove? Do you remove the oldest one? The one you haven't touched in a while? Or do you draw a lottery ticket to decide? **This is what we are building.**

---

##  Technical Architecture

### Key Components
-   **Page Tables (`pgdir`):** Every process has a map translating Virtual Addresses (VA) to Physical Addresses (PA).
-   **The MMU:** The hardware that reads `pgdir`. If a page is missing (`PTE_P == 0`), it triggers a **Page Fault**.

### The Life of a Memory Access
1.  **Access:** CPU tries to access a Virtual Address.
2.  **Fault:** If the page is not in RAM, the MMU raises a Page Fault exception.
3.  **Trap:** The OS kernel catches the fault in `trap()` and calls `handle_pgfault()`.
4.  **Decision:**
    -   **New Page:** Allocate a fresh frame.
    -   **Swapped Page:** Read data back from the disk (Swap-In).
5.  **Eviction (The Critical Step):** If RAM is full, `select_a_victim()` runs to pick a page to kick out (Swap-Out) to make space.
6.  **Resume:** The page table is updated, and the user instruction restarts as if nothing happened.

### Key Files Modified
-   **`vm.c`**: The heart of VM. Contains `select_a_victim()` (our main target).
-   **`trap.c`**: Handles timer interrupts (crucial for updating lottery tickets later).
-   **`bio.c` / `fs.c`**: Low-level disk operations for swapping pages.

---

##  Development Environment & Fixes

We are running this project on **WSL2 (Ubuntu)** using a modern GCC toolchain. Since the codebase is older, several stability fixes were applied:

### Build Environment
-   **OS:** Windows Subsystem for Linux (WSL2)
-   **Compiler:** GCC (`build-essential`)
-   **Emulator:** QEMU (`qemu-system-x86`)

### Compatibility Patches
To ensure a clean build with modern compilers (`-Werror`), we applied the following:
1.  **`vm.c` Loop Fix:** Changed a loop counter from `long` to `uint`. (Iterating virtual memory with signed types causes undefined behavior in modern GCC optimization).
2.  **Makefile Flags:** Added `-Wno-array-bounds` and `-Wno-infinite-recursion` to silence benign warnings treated as errors.
3.  **Script Permissions:** Added execution rights to `sign.pl`.

---

##  Execution & Verification

To build and boot the system:
```bash
make clean
make
make qemu-nox
```

### Baseline Validation
Before implementing new algorithms, we verified the system using existing tests:
-   `memtest1`: Validates Demand Paging logic.
-   `memtest2`: Validates Swapping and `fork()` under memory pressure.

---

## Project Roadmap

### Phase 0: Baseline Analysis (Completed)
The default xv7 uses a **Simple Clock (Stateless Second-Chance)** algorithm. It scans for a page with `accessed_bit == 0` but restarts the scan from the beginning every time.

### Phase 1: FIFO Implementation (Next Step)
We will replace the default algorithm with a **First-In, First-Out (FIFO)** queue.
-   **Goal:** Ensure the oldest loaded page is evicted first.
-   **Why:** To establish a deterministic baseline and warm up with the `vm.c` code structure.

### Phase 2: LotteryVM Implementation (Final Goal)
We will implement a probabilistic **Lottery-Based Replacement** policy.
-   **Infrastructure:** Add ticket metadata to pages (`struct core_map`).
-   **The Scanner:** Update tickets periodically in `trap.c` based on page usage (aging).
-   **The Dealer:** Rewrite `select_a_victim()` to run an "Inverse Lottery" to pick a victim.


---