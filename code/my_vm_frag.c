#include "my_vm.h"
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h> 
#include <string.h>


#define get_top_bits(x, y) ((x) >> (32 - y))

#define get_middle_bits(x, y, z) ((x >> (z)) & ((1UL << (y)) - 1))

#define get_bottom_bits(x, y) (((1UL << y) - 1) & (x))

typedef uint16_t header_t;

#define CREATE_HEADER(f, s) (((s) << 1) | (f))
#define HEADER_GET_SIZE(h) ((h) >> 1)
#define HEADER_GET_FREE(h) ((h) & 1)
#define HEADER_GET_NEXT(h) ((h) + ((*(header_t*)h) >> 1))

// Physical memory array. The size is defined by MEMSIZE
void * physical_memory = NULL;

// Bitmaps
unsigned char *phys_page_bmap;
unsigned char *virt_page_bmap;
// unsigned char *malloc_allocated;
unsigned char *alloc_page_bmap;
unsigned char *full_page_bitmap;


// page directory
pde_t* pgdir;

// TLB
struct tlb_entry tlb_store[TLB_ENTRIES];
unsigned int tlb_hit = 0;
unsigned int tlb_miss = 0;
unsigned int tlb_total = 0;


// TLB Mutex
pthread_mutex_t tlb_lock;

// Malloc and Free Mutex
pthread_mutex_t malloc_free_lock;

// Bits for each section of mem address
unsigned int offset_bits;
unsigned int pd_bits;
unsigned int pt_bits;

// Highest allocated page index
int highest_pg_index = 2;

void *get_next_avail(int num_pages, int isUser);

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

// Calcualtes log 2 without floats or math library
int int_log2(int num) {
    int log2 = 0;

    // Right shift until num becomes 0
    while (num >>= 1) {
        log2++;
    }
    return log2;
}


/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {
    // For a 32 bit

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them

    // Initialize n_malloc and free lock for thread safety  
    if (pthread_mutex_init(&malloc_free_lock, NULL) != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }

    physical_memory = mmap(NULL, MEMSIZE, PROT_READ | PROT_WRITE,  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (physical_memory == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    memset(physical_memory, 0, MEMSIZE);


    for (int i = PGSIZE; i < MEMSIZE; i+=PGSIZE) {
        CREATE_HEADER(1, PGSIZE);
    }

    size_t phys_size = ((NUM_PHYS_PAGES + 7) / 8) * 8;
    phys_page_bmap = (unsigned char *)malloc(phys_size);
    if (phys_page_bmap == NULL) {
        perror("malloc phys_bmap failed");
        exit(EXIT_FAILURE);
    }

    memset(phys_page_bmap, 0, phys_size);

    
    size_t virt_size = ((NUM_VIRT_PAGES + 7) / 8) * 8;
    virt_page_bmap = (unsigned char *)malloc(virt_size);
    if (virt_page_bmap == NULL) {
        perror("malloc virt_bmap failed");
        exit(EXIT_FAILURE);
    }

    memset(virt_page_bmap, 0, virt_size);

    // Allows us to check if the process accesses memory not allocated to it 
    alloc_page_bmap = (unsigned char *)malloc(virt_size);
    if (alloc_page_bmap == NULL) {
        perror("alloc_page_bmap failed");
        exit(EXIT_FAILURE);
    }

    memset(alloc_page_bmap, 0, virt_size);

    // Allows us to check a allocated page is full (therefore will not have any headers/no possibility for fragmentation) 
    full_page_bitmap = (unsigned char *)malloc(virt_size);
    if (full_page_bitmap == NULL) {
        perror("full_page_bitmap failed");
        exit(EXIT_FAILURE);
    }

    memset(full_page_bitmap, 0, virt_size);

    // Initialize Page Directory, we will set as first page in our physical memory.
    // equivalent to pde_t array of length PAGE_TABLE_ENTRIES_PER_LEVEL

    pgdir = (pte_t*) physical_memory;

    set_bit_at_index(phys_page_bmap, 0);
    set_bit_at_index(virt_page_bmap, 0); 
    set_bit_at_index(full_page_bitmap, 0); 
    set_bit_at_index(alloc_page_bmap, 0); 

    // Grabs # of bits for each three sections of address based on page size
    offset_bits = int_log2(PGSIZE);
    pd_bits = (32 - offset_bits) / 2;
    pt_bits = 32 - pd_bits - offset_bits;

    // Initialize TLB lock for thread safety
    if (pthread_mutex_init(&tlb_lock, NULL) != 0) {
        perror("pthread_mutex_init failed");
        exit(EXIT_FAILURE);
    }
}

int TLB_hash(unsigned long va) {
    int top_bits = get_top_bits(va, offset_bits);
    return (int) va % TLB_ENTRIES;
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 * 
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures touched when interacting with the TLB
 */
int TLB_add(void *va, void *pa)
{
    if(va == NULL || pa == NULL) {
        return -1;
    }

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    pthread_mutex_lock(&tlb_lock);

    // Strip the offsets and store the vpns stripped
    unsigned long vpn = ((unsigned long)va) & ~((1 << offset_bits) - 1);
    unsigned long pfn = ((unsigned long)pa) & ~((1 << offset_bits) - 1);


    int hash = TLB_hash(vpn);
    struct tlb_entry new_entry = {vpn, pfn, 1};
    tlb_store[hash] = new_entry;
    pthread_mutex_unlock(&tlb_lock);

    return 0;
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
    pthread_mutex_lock(&tlb_lock);
    tlb_total++;

    // Check if the entry is valid and the vpn matches the va, mask the offset bits from the va
    unsigned long va_b = ((unsigned long)va) & ~((1 << offset_bits) - 1);

    int hash = TLB_hash(va_b);
    struct tlb_entry entry = tlb_store[hash];

    

    if (entry.valid == 1 && entry.vpn == va_b) {
        tlb_hit++;
        pthread_mutex_unlock(&tlb_lock);
        return (pte_t *)entry.pfn;
    }

    tlb_miss++;
    pthread_mutex_unlock(&tlb_lock);
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

    if (tlb_total != 0) {
        miss_rate = (double)tlb_miss / tlb_total;
    }

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
void *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
   // Only for 32-bit systems so far

    if(va == NULL) {
        return NULL;
    }

     // Get the page indexes
    unsigned long vaddress = (unsigned long)va;
    unsigned long offset = get_bottom_bits(vaddress, offset_bits);

    pte_t phys_addr = (unsigned long) TLB_check(va);

    // All of these addresses return with some arbitrary offset
    if(phys_addr != NULL) {
        phys_addr = (phys_addr & ~((1 << offset_bits) - 1)) | offset;
        return (void *)phys_addr;
    }

    unsigned long pd_index = get_top_bits(vaddress, pd_bits);
    unsigned long pt_index = get_middle_bits(vaddress, pt_bits, offset_bits);

    // Get the page table entry
    pte_t* page_table = (pte_t*) pgdir[pd_index];
    if (page_table == NULL) {
        printf("Page table not allocated\n");
        return NULL;
    }

    pte_t phys_page_ptr = page_table[pt_index];
    

    if (phys_page_ptr == 0) {
        return NULL;
    }

    // Use binary OR to combine the physical page and the offset
    unsigned long physical_address = (phys_page_ptr & ~((1 << offset_bits) - 1)) | offset;

    TLB_add(va, (void *)physical_address);
    return (void *)physical_address;
}


void* get_next_avail_phys() {
        unsigned long free_phys_page_index = 0;
        int j = 1;

        while (free_phys_page_index == 0 && j < NUM_PHYS_PAGES) {
            if (get_bit_at_index(phys_page_bmap, j) == 0) {
                free_phys_page_index = j;
            }
            j++;
        }

        if (free_phys_page_index == 0) {
            return NULL;
        }

        set_bit_at_index(phys_page_bmap, free_phys_page_index);

        void* physical_address = (void*)(((uint8_t*)physical_memory) + free_phys_page_index*PGSIZE);

        return physical_address;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int map_page(pde_t *pgdir, void *va, void *pa)
{
    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

    // Get the page directory index
    unsigned long vaddress = (unsigned long)va;
    unsigned long pd_index = get_top_bits(vaddress, pd_bits);
    unsigned long pt_index = get_middle_bits(vaddress, pt_bits, offset_bits);


    // Get the page table entry
    pte_t* page_table = (pte_t*) pgdir[pd_index];
    if (page_table == NULL) {
        // Allocate page to this new page table, need to find free page

        void* virt_page_address = get_next_avail(PGSIZE, 0);
        void* phys_page_address = get_next_avail_phys();

        if (virt_page_address == NULL || phys_page_address == NULL) {
            return -1;
        }

        pgdir[pd_index] = (pde_t) phys_page_address;
        page_table = (pte_t*) pgdir[pd_index];
    }


    void* phys_page_ptr = (void*) page_table[pt_index];

    if (phys_page_ptr == NULL) {
        // Connect virtual address to physical address 
        page_table[pt_index] = (pte_t) pa;

    }


    return 0;
}


/*Function that gets the next available page
*/
// void *get_next_avail(int num_pages, int isUser) {
//     //Use virtual address bitmap to find the next free page

//     int virt_page_num =  ((NUM_VIRT_PAGES + 7) / 8) * 8;

//     // First page is for directory, start at 1
//     unsigned long bit_index;
//     int found = 0;
//     for (bit_index = 1; bit_index < MAX_MEMSIZE; bit_index++) {
//         if (get_bit_at_index(virt_page_bmap, bit_index) == 0) {
//             int count = 1;
//             for (int j = 1; j < num_pages; j++) {
//                 if (get_bit_at_index(virt_page_bmap, bit_index+j) == 0) {
//                     count += 1;
//                 }
//             }
//             if (count == num_pages) {
//                 found = 1;
//                 break;
//             }
//             bit_index += num_pages-1;
//         }
//     }

//     if (found == 0) {
//         return NULL;
//     }

//     // marks as allocated
//     for (int k = 0; k < num_pages; k++) {
//         set_bit_at_index(virt_page_bmap, k+bit_index);
//         if (isUser == 1) {
//             set_bit_at_index(malloc_allocated, k+bit_index);    
//         }
//     }

//     // bit_index gives us pdi and pti, we can set offset bits 0 (left shift)
//     unsigned long virtual_address = bit_index << offset_bits;

//     return (void*) virtual_address;
// }

//merges two blocks together and sets to freed
static void mergeBlocks(char* firstBlock, char* secondBlock) {
	*(header_t*)firstBlock = CREATE_HEADER(1, HEADER_GET_SIZE(*(header_t*)firstBlock) + HEADER_GET_SIZE(*(header_t*)secondBlock));
}

//checks if block after pointer is freed, if so, gets merged with head
static void checkNextBlock(char* head, char* pointer) {
    char* physMemoryEnd = (char*)physical_memory + MEMSIZE;
	if ((HEADER_GET_NEXT(pointer) < physMemoryEnd) && HEADER_GET_FREE(*(header_t*)HEADER_GET_NEXT(pointer))) {
		mergeBlocks((head), HEADER_GET_NEXT(pointer));
	}
}

void *get_next_avail(int num_bytes, int isUser) {
    // If allocation takes up multiple pages or entirety of one page, we will not fragment
    int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;
    
    // The lower bound of num_bytes is set based on the idea
    // That if we fragment any larger of a buffer, we will have no room for a header + data
    if (num_pages > 1 || num_bytes >= (PGSIZE-4)) {
        int found = 0;
        unsigned long bit_index;
        for (bit_index = 1; bit_index < MAX_MEMSIZE; bit_index++) {
            if ((get_bit_at_index(virt_page_bmap, bit_index) == 0) && (get_bit_at_index(alloc_page_bmap, bit_index) == 0)) {
                int count = 1;
                for (int j = 1; j < num_pages; j++) {
                    if (get_bit_at_index(virt_page_bmap, bit_index+j) == 0) {
                        count += 1;
                    }
                }
                if (count == num_pages) {
                    found = 1;
                    break;
                }
                bit_index += num_pages-1;
            }
        }

        if (found == 0) {
            return NULL;
        }

        // marks as allocated
        for (int k = 0; k < num_pages; k++) {
            set_bit_at_index(virt_page_bmap, k+bit_index);
            set_bit_at_index(full_page_bitmap, k+bit_index);
            if (isUser == 0) {
                set_bit_at_index(alloc_page_bmap, k+bit_index);    
            }
        }

        highest_pg_index = (num_pages + bit_index > highest_pg_index) ? num_pages + bit_index : highest_pg_index;

        unsigned long virtual_address = bit_index << offset_bits;
        return (void*) virtual_address;
    }

    // We explore fragmentation

    unsigned long page_index;
    int found = 0;
    void* first_new_full_page = NULL;

    int needed_bytes = num_bytes + sizeof(header_t);
    for (page_index = 1; page_index < highest_pg_index+1; page_index++) {
        if (get_bit_at_index(full_page_bitmap, page_index) == 0) {
            // We check if this page has some free bytes left
            // If not, we reset to byte_count to 0 and find a new chunk
            char* va = (char*) (page_index << offset_bits);
            char* pa = (char*) translate(pgdir, va);
            char* pa_old = pa;
                        // printf("page index: %d, virtual_address: %x, physical : %x\n",page_index, va, pa);
            printf("pa : %x\n", pa);
            if (pa == NULL) {
                // not mapped yet, we will save if no fragmented pages exist
                if (first_new_full_page == NULL) {
                    printf("first_new_full_page: %d\n", va);
                    first_new_full_page = va;
                }
                continue;
            }
            // grabs first header of page block (address 0 of page)
            header_t curHeader = *(header_t*)pa;
            while ((!HEADER_GET_FREE(curHeader) || (HEADER_GET_SIZE(curHeader) < needed_bytes)) && (pa + HEADER_GET_SIZE(curHeader)) < ((char*)pa_old + PGSIZE)) {
                // printf("va: %x, header size: %x, page_index: %d, pa_old: %x, pa: %x, max: %x\n", va, HEADER_GET_SIZE(curHeader), page_index,pa_old, pa, ((char*)pa + PGSIZE));
                pa += HEADER_GET_SIZE(curHeader);
                va += HEADER_GET_SIZE(curHeader);
                curHeader = *(header_t*)pa;
            }

            // checks if we ran out of mem or no block large enough
            if(!HEADER_GET_FREE(curHeader) || HEADER_GET_SIZE(curHeader) < needed_bytes)
            {
                continue;
            }

            // splits block if too large
            if ((HEADER_GET_SIZE(curHeader)) > needed_bytes) {
                *(header_t*)(pa + needed_bytes) = CREATE_HEADER(1, HEADER_GET_SIZE(curHeader) - needed_bytes);
            }

            //set current block to allocated and return:
            *(header_t*)pa = CREATE_HEADER(0, needed_bytes);
            printf("virtual_address: %x", va);
	        return (void*)(va + sizeof(header_t));
        }
    }

    printf("bazing\n");
    
    // sets page as allocated
    set_bit_at_index(virt_page_bmap, (unsigned long)first_new_full_page >> offset_bits);

    // If no fragmented pages are large enough, we write to a new page
    char* new_pa =(char*) get_next_avail_phys();

    // Setup two blocks
    printf("needed byte: %d\n", needed_bytes);
    *(header_t*)new_pa = CREATE_HEADER(0, needed_bytes);
    *(header_t*)(new_pa + needed_bytes) = CREATE_HEADER(1, PGSIZE - needed_bytes);

    printf("header size: %d\n", HEADER_GET_SIZE(*(header_t*)new_pa));


    map_page(pgdir, first_new_full_page, new_pa);

    highest_pg_index = ((unsigned long)first_new_full_page >> offset_bits > highest_pg_index) ? (unsigned long)first_new_full_page >> offset_bits : highest_pg_index;

    return (void*)((char *)first_new_full_page + sizeof(header_t));

}

pthread_once_t once_control = PTHREAD_ONCE_INIT;

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
    
    pthread_once(&once_control, set_physical_mem);
    pthread_mutex_lock(&malloc_free_lock);

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    // Rounds to smallest # of pages to fit num_bytes
    int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;

    // uint8_t type casting allows for pointer arithmetic
    uint8_t* virtual_address = (uint8_t*)get_next_avail(num_bytes, 1);

    if (virtual_address == NULL) {
        pthread_mutex_unlock(&malloc_free_lock);
        printf("No avaliable memory left for %d bytes\n", num_bytes);
        return NULL;
    }
    
    // If address is a subsection of a page, we already did physical allocaiton in get_next_avail
    if ((unsigned long) virtual_address % PGSIZE != 0) {
        pthread_mutex_unlock(&malloc_free_lock);
        printf("before seg: %x\n", virtual_address);
        return (void*) virtual_address;    
    }

    // Each virtual address should correlate to a physical page
    // The physical pages do not need to be contiguous
    // Loops through virtual pages we found and find free physical page to map to
    for (int i = 0; i < num_pages; i++) {

        void* physical_address = get_next_avail_phys();

        if (physical_address == NULL) {
            pthread_mutex_unlock(&malloc_free_lock);
            printf("No avaliable physical memory left for %d bytes\n", num_bytes);      
            return NULL;      
        }

        if (map_page(pgdir, virtual_address+i, physical_address) == -1) {
            pthread_mutex_unlock(&malloc_free_lock);
            printf("Mapping page failed\n");
            return NULL;
        }
    }

    pthread_mutex_unlock(&malloc_free_lock);
    return (void*) virtual_address;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void n_free(void *va, int size) {
    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is v 
     * Part 2: Also, remove the translation from the TLB
     */

    unsigned long vaddress = (unsigned long)va;


    // fragmented free
    if (vaddress % PGSIZE != 0) {

        char* start = ((unsigned long)va) & ~((1 << offset_bits) - 1);

        unsigned long offset = get_bottom_bits(vaddress, offset_bits);

        // Check if address in in TLB
        void* pageStart = (void*) TLB_check((void *)(vaddress));
        if(pageStart == NULL) {
            // If not, translate and add to TLB
            pageStart = translate(pgdir, va);
            if(pageStart == NULL) {
                printf("get_data failed\n");
                return;
            }
            TLB_add(vaddress, pageStart);
            pageStart = ((unsigned long)pageStart) & ~((1 << offset_bits) - 1);
        }

        char* ptr = pageStart + offset;
        
        char* memoryEnd = (char*)pageStart + PGSIZE;

        //grabs first block from page
        char* curMem = (char*)pageStart;

        //traverse blocks to find pointer or free block adjacent to pointer
        while (curMem < memoryEnd) {	

            // checks if current pointer is free and is before target pointer
            if (
                HEADER_GET_NEXT(curMem) < memoryEnd &&
                HEADER_GET_FREE(*(header_t*)curMem) && 
                HEADER_GET_NEXT(curMem) == ((char*)ptr - sizeof(header_t)) && 
                !HEADER_GET_FREE(*(header_t*)((char*)ptr - sizeof(header_t)))
                ) {
                char* pointerHeader = (char*)ptr - sizeof(header_t);

                mergeBlocks(curMem, pointerHeader);

                // checks if block after pointerHeader is free, then merges if so
                checkNextBlock(curMem, pointerHeader);

                return;
		    }


            // checks if current pointer is target pointer
            if ( curMem == ((char*)ptr - sizeof(header_t)) && !HEADER_GET_FREE(*(header_t*)((char*)ptr - sizeof(header_t)))) {

                // checks if block after curMem is free, then merges if so
                checkNextBlock(curMem, curMem);

                // sets new block to freed
                *(header_t*)curMem = CREATE_HEADER(1, HEADER_GET_SIZE(*(header_t*)curMem));
                
                return;
            }

            curMem = HEADER_GET_NEXT(curMem);
	    }
        printf("n_free failed\n");
        return;
    }

    int bit_index;
    for (int i = 0; i < size && vaddress < MAX_MEMSIZE; i++) {
        bit_index = vaddress >> offset_bits;
        // Avoid freeing unallocated memory and memory not allocated by process
        if (get_bit_at_index(virt_page_bmap, bit_index) != 1 || get_bit_at_index(alloc_page_bmap, bit_index) == 1) {
            printf("n_free failed\n");
            return;
        }
        vaddress += 1;
    }

    // Check that va+size isn't over our bounds
    if (vaddress >= MAX_MEMSIZE) {
        printf("n_free failed\n");
        return;        
    }

    // We can now free with confidence
    int prev_bit_index = -1;
    vaddress = (unsigned long)va;
     for (int i = 0; i < size && vaddress < MAX_MEMSIZE; i++) {
        bit_index = vaddress >> offset_bits;
        if (prev_bit_index != bit_index) {
            set_bit_at_index(virt_page_bmap, bit_index);
        }
        prev_bit_index = bit_index;
        vaddress += 1;
    }
    // invalidate TLB
    pthread_mutex_lock(&tlb_lock);
    if(tlb_store[TLB_hash((unsigned long)va)].vpn == ((unsigned long)va & ~((1 << offset_bits) - 1))) {
        tlb_store[TLB_hash((unsigned long)va)].valid = 0;
    }
    pthread_mutex_unlock(&tlb_lock);
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

    unsigned long vaddr = (unsigned long) va;
    unsigned char *src = (unsigned char *)val;
    int written = 0;

    if(!vaddr || !src ) {
        printf("put_data failed");
        return -1;
    }
    
    while(written < size) {
        // Each time it returns here, recalculate for the next physical page addr.
        char *physical_address = (char *)translate(pgdir, (void *)vaddr);
        if (physical_address == NULL) {
            printf("put_data failed\n");
            return -1;
        }

        // Gives us the amount of bytes we can copy to the page
        int page_offset = vaddr % PGSIZE;
        int bytes_to_copy = PGSIZE - page_offset;
        if (bytes_to_copy > (size - written)) {
            bytes_to_copy = size - written;
        }

        if (get_bit_at_index(alloc_page_bmap, vaddr>>offset_bits) == 1) {
            printf("put_data failed: memory allocated\n");
            return -1;
        }

        memcpy(physical_address, src, bytes_to_copy);
        // printf("WE WANT 1: %d\n", *(int*)physical_address);
        written += bytes_to_copy;
        src += bytes_to_copy;
        vaddr += bytes_to_copy;
    }



    /*return -1 if put_data failed and 0 if put is successfull*/
    return 0;
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_data(void *va, void *val, int size) {
    if(va == NULL || val == NULL || size <= 0) {
        printf("get_data failed: invalid parameters\n");
        return;
    }


    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */

    int read = 0;
    
    unsigned long offset = get_bottom_bits((unsigned long)va, offset_bits);
    unsigned long vaddr = (unsigned long)va;

    while(read < size) {
        unsigned long offset = get_bottom_bits(vaddr, offset_bits);

        // Check if address in in TLB
        void* physical_address = (void*) TLB_check((void *)(vaddr));
        if(physical_address == NULL) {
            // If not, translate and add to TLB
            physical_address = translate(pgdir, va);
            if(physical_address == NULL) {
                printf("get_data failed\n");
                return;
            }
            TLB_add(vaddr, physical_address);
        } else {
            physical_address += offset;
        }

        // Check if size is greater than the page size
        int page_offset = vaddr % PGSIZE;
        int bytes_to_copy = PGSIZE - page_offset;
        if(bytes_to_copy > (size - read)) {
            bytes_to_copy = size - read;
        }

        if(get_bit_at_index(alloc_page_bmap, vaddr>>offset_bits) == 1) {
            printf("get_data failed: memory not allocated\n");
            pthread_mutex_unlock(&malloc_free_lock);
            return;
        }

        unsigned char *dst = (unsigned char *)val;
        memcpy(val, (void *)physical_address, bytes_to_copy);
        // printf("WE HAVE 1: %d\n", *(int*)physical_address);
        read += bytes_to_copy;
        dst += bytes_to_copy;
        vaddr += bytes_to_copy;
    }
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
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            put_data((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

