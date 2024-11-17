#include "my_vm.h"
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h> 


#define get_top_bits(x, y) ((x) >> (32 - y))

#define get_middle_bits(x, y, z) ((x >> (z)) & ((1UL << (y)) - 1))

#define get_bottom_bits(x, y) (((1UL << y) - 1) & (x))

// Physical memory array. The size is defined by MEMSIZE
void * physical_memory;
// Bitmaps
unsigned char *phys_bmap;
unsigned char *virt_bmap;

// pg directory
pde_t *pgdir;

// TLB Mutex
pthread_mutex_t tlb_lock;

static void set_bit_at_index(char *bitmap, int index)
{
    //Little endian
    size_t block_index = index / 8;
    bitmap[block_index] = bitmap[block_index] ^ (1 << (index - block_index * 8));

    return;
}


/* 
 * GETTING A BIT AT AN INDEX 
 * Function to get a bit at "index"
 */
static int get_bit_at_index(char *bitmap, int index)
{
    //Get to the location in the character bitmap array
    //Little endian
    size_t block_index = index / 8;
    return (bitmap[block_index] >> (index - block_index * 8)) & 0x1;

}


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    unsigned int num_phys = MEMSIZE / PGSIZE;  // 1GB / 4KB = 262144
    unsigned int num_virt = MAX_MEMSIZE / PGSIZE; // 4GB / 4KB = 1048576

    physical_memory = mmap(NULL, MEMSIZE, PROT_READ | PROT_WRITE,  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (physical_memory == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    size_t phys_size = (num_phys + 7) / 8;
    phys_bmap = (unsigned char *)malloc(phys_size);
    if (phys_bmap == NULL) {
        perror("malloc phys_bmap failed");
        exit(EXIT_FAILURE);
    }

    memset(phys_bmap, 0, phys_size);

    
    size_t virt_size = (num_virt + 7) / 8;
    virt_bmap = (unsigned char *)malloc(virt_size);
    if (virt_bmap == NULL) {
        perror("malloc virt_bmap failed");
        exit(EXIT_FAILURE);
    }

    memset(virt_bmap, 0, virt_size);

    // Set pd directory to the physical memory
    pgdir = (pde_t *)physical_memory;

    set_bit_at_index(phys_bmap, 0);
    set_bit_at_index(virt_bmap, 0); 

    // Initialize TLB lock for thread safety
    if (pthread_mutex_init(&tlb_lock, NULL) != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }
}


/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 * 
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures touched when interacting with the TLB
 */
int
TLB_add(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */

    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 * 
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures touched when interacting with the TLB
 */
pte_t *
TLB_check(void *va) {

    /* Part 2: TLB lookup code here */



   /*This function should return a pte_t pointer*/

   return NULL;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;	

    /*Part 2 Code here to calculate and print the TLB miss rate*/




    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
   // Only for 32-bit systems so far
    
    // Get the page directory index
    unsigned long vaddress = (unsigned long)va;
    unsigned long pd_index = get_top_bits(vaddress, 10);
    unsigned long pt_index = get_middle_bits(vaddress, 10, 12);
    unsigned long offset = get_bottom_bits(vaddress, 12);

    // Get the page table entry
    pte_t *page_table = (pte_t *)pgdir[pd_index];
    if (page_table == NULL) {
        return NULL;
    }

    pte_t physical_page = page_table[pt_index];
    if ((physical_page & 0x1) == 0) {
        return NULL;
    }

    // Use binary OR to combine the physical page and the offset
    unsigned long physical_address = (physical_page & ~0xFFF) | offset;

    return (pte_t *)physical_address;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int 
map_page(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page

    return NULL;
}


/* Function responsible for allocating pages and used by the benchmark
 *
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures you plan on updating. Two threads calling
 *       this function at the same time should NOT get the same
 *       or overlapping allocations
*/
void *n_malloc(unsigned int num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    return NULL;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void n_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
    
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
int put_data(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */


    /*return -1 if put_data failed and 0 if put is successfull*/

    return -1;
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_data(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_data() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_data( (void *)address_a, &a, sizeof(int));
                get_data( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_data((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}



