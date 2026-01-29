// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"
#include "coremap.h"

// Calculate the number of frames based on total physical memory size.
// core_map acts as a parallel array to physical memory frames.
struct core_map_entry core_map[PHYSTOP >> PGSHIFT];


// Global clock tick for the FIFO algorithm.
// Increments on every page allocation to represent relative time.
unsigned int fifo_clock = 0;


void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);        //to add pages to freelist
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);        //to add pages to freelist
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree in kalloc.c");

  // --- FIFO LOGIC START ---
  // Reset metadata when a page is freed (returned to the free list).
  uint pa = V2P(v);             // Convert Kernel Virtual Address to Physical Address
  uint idx = pa >> PGSHIFT;     // Calculate frame index (Page Frame Number)
  
  core_map[idx].is_allocated = 0; // Mark frame as free
  core_map[idx].birth_time = 0;   // Clear timestamp
  // --- FIFO LOGIC END ---

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);
  
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    // --- FIFO LOGIC START ---
    // Initialize metadata for the newly allocated page.
    uint pa = V2P((char*)r);      // Get the physical address of the allocated block
    uint idx = pa >> PGSHIFT;     // Calculate frame index
    
    core_map[idx].is_allocated = 1;       // Mark frame as allocated
    core_map[idx].birth_time = fifo_clock++; // Assign current timestamp and increment clock
    // --- FIFO LOGIC END ---
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}
