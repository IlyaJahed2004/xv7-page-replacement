#ifndef XV7_COREMAP_H
#define XV7_COREMAP_H

/**
 * struct core_map_entry
 * Represents metadata for a single physical page frame in RAM.
 * Used by the page replacement algorithm to track page usage.
 */
struct core_map_entry {
  int is_allocated;        // Flag: 1 if the frame is currently in use, 0 otherwise
  unsigned int birth_time; // Timestamp: Records when the page was allocated (for FIFO)
};

// Global array declaration.
// Maps physical frame indices to their metadata.
extern struct core_map_entry core_map[];

#endif
