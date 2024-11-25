#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE (4096)
// #define PGSIZE 16384


// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

#define NUM_PHYS_PAGES MEMSIZE / PGSIZE
#define NUM_VIRT_PAGES MAX_MEMSIZE / PGSIZE

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

struct tlb_entry {
    uint32_t vpn;
    uint32_t pfn;
    bool valid;
};

// //Structure to represents TLB
// struct tlb {
//     /*Assume your TLB is a direct mapped TLB with number of entries as TLB_ENTRIES
//     * Think about the size of each TLB entry that performs virtual to physical
//     * address translation.
//     */
//     struct tlb_entry entries[TLB_ENTRIES];
// };
// struct tlb tlb_store;

// Setup functions
void set_physical_mem();

// TLB Functions
int TLB_add(void *va, void *pa);
pte_t *TLB_check(void *va);
void print_TLB_missrate();

// Page Table Functions
void* translate(pde_t *pgdir, void *va);
int map_page(pde_t *pgdir, void *va, void* pa);

// Allocation functions
void *n_malloc(unsigned int num_bytes);
void n_free(void *va, int size);

// Data operations
int put_data(void *va, void *val, int size);
void get_data(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
#endif