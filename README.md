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


## Phase 1: Architecture Analysis & FIFO Logic

Before diving into the implementation of the First-In-First-Out (FIFO) eviction policy, we analyzed how xv7 manages physical memory and how the kernel views addresses. This understanding is critical for correctly implementing the `core_map` logic.

### 1. The `core_map`: A Physical Memory Ledger
The `core_map` acts as the "Property Deed" registry for the RAM. 
*   **The Concept:** At the Physical Memory Manager level, the kernel focuses on the **container** (the frame) rather than the content.
*   **The Structure:** We utilize an array where every index corresponds directly to a physical frame number (PFN).
    *   `core_map[0]` $\rightarrow$ Physical Frame 0
    *   `core_map[i]` $\rightarrow$ Physical Frame `i`
*   **The Metadata:** For every frame, we track:
    1.  `is_allocated`: Is this frame currently in use?
    2.  `birth_time`: When was this frame allocated? (Crucial for FIFO).

### 2. The `kalloc` Workflow
A common misconception is that `kalloc` translates addresses. In reality, `kalloc` is a **distributor**.

1.  **The Source:** `kalloc` pulls a free page from `kmem.freelist`. These pages are already mapped into the Kernel's Virtual Address space.
2.  **The Return Value:** It returns a **Virtual Address** pointer (e.g., `0x80200000`). This is necessary because the CPU (and the kernel code running on it) always sees memory through the MMU.
3.  **The Challenge:** To update our `core_map` (which tracks physical frames), we need the **Physical Address**, not the virtual one returned by `kalloc`.

### 3. Deep Dive: Kernel Virtual vs. Physical Addresses
The xv7 kernel uses a "Direct Mapping" strategy for physical memory.

*   **The "MMU Goggles":** The CPU never sees raw physical memory. It views memory through a virtual mapping.
*   **The Offset:** The kernel maps all available physical RAM to a high virtual address range starting at `KERNBASE` (usually `0x80000000` or 2GB).

**The Translation Formula:**
> Physical Address = Kernel Virtual Address - KERNBASE

### Visualizing the Allocation Flow
When `kalloc()` is called to allocate a new page, the following translation occurs to update the ledger:
```text
Step 1: kalloc retrieves a free page pointer (Virtual Address)
Pointer 'r' = 0x80001000  (Kernel Space)

Step 2: We need the Physical Index for core_map
We apply V2P (Virtual to Physical macro):
0x80001000 - 0x80000000 (KERNBASE) = 0x00001000 (Physical Addr)

Step 3: Calculate Index
Physical Addr 0x1000 / PageSize (4096) = Index 1

Step 4: Update Ledger
core_map[1].birth_time = current_clock;
core_map[1].is_allocated = 1;

Step 5: Return Virtual Pointer
The function returns 'r' (0x80001000) so the kernel can write to it.
```

### 4. The Eviction Strategy (FIFO)
When memory is full (`select_a_victim` in `vm.c`), we perform a linear scan of the page tables (virtual space):
1.  We look at a candidate page (PTE).
2.  We extract the physical address from the PTE.
3.  We check the `core_map` using that physical address to retrieve its `birth_time`.
4.  We compare it against the oldest `birth_time` found so far.
5.  The page with the smallest (earliest) timestamp is selected for eviction.


In this phase, we replaced the default circular replacement policy with a **First-In, First-Out (FIFO)** algorithm. The goal ensures that the page which has been in the physical memory the longest is the first one to be evicted when a swap is necessary.

### 1. Data Structures (`coremap.h`)
We introduced a `core_map` structure to store metadata for every physical page frame.
```c
struct core_map_entry {
  int is_allocated;        // Tracks if the frame is in use
  unsigned int birth_time; // Timestamp of when the page was loaded (FIFO)
};
```

### 2. Time Tracking (`kalloc.c`)
To determine the "age" of a page, we implemented a global clock:
-   **`fifo_clock`**: A global counter that increments every time a physical page is allocated.
-   **Allocation Hook (`kalloc`)**: When a page is allocated, we record the current `fifo_clock` into `core_map[frame_index].birth_time`.
-   **Free Hook (`kfree`)**: When a page is freed, we reset its metadata in the `core_map`.

### 3. Eviction Logic (`vm.c`)
The `select_a_victim()` function was rewritten to scan the page table. Instead of stopping at the first non-accessed page, it now:
1.  Iterates through all present user pages (`PTE_P` and `PTE_U`).
2.  Looks up the `birth_time` of the corresponding physical frame in the `core_map`.
3.  Selects the page with the **minimum** `birth_time` (the oldest page) as the victim for eviction.

This establishes the infrastructure required for the Lottery algorithm, where `birth_time` will eventually be replaced or augmented by `tickets`.

---