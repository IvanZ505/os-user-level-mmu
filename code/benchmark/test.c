#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "../my_vm.h"

#define SIZE 5
#define ARRAY_SIZE 400

int main() {

    printf("Allocating three arrays of %d bytes\n", ARRAY_SIZE);

    void *a = n_malloc(ARRAY_SIZE);
    int old_a = (int)a;
    void *b = n_malloc(ARRAY_SIZE);
    void *c = n_malloc(ARRAY_SIZE);
    int x = 1;
    int y, z;
    int i =0, j=0;
    int address_a = 0, address_b = 0;
    int address_c = 0;

    printf("Addresses of the allocations: %x, %x, %x\n", (int)a, (int)b, (int)c);

    printf("Storing integers to generate a SIZExSIZE matrix\n");
    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            put_data((void *)address_a, &x, sizeof(int));
            put_data((void *)address_b, &x, sizeof(int));
        }
    } 

    printf("Fetching matrix elements stored in the arrays\n");

    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_a = (unsigned int)a + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            address_b = (unsigned int)b + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            get_data((void *)address_a, &y, sizeof(int));
            get_data( (void *)address_b, &z, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    } 

    printf("Performing matrix multiplication with itself!\n");
    mat_mult(a, b, SIZE, c);


    for (i = 0; i < SIZE; i++) {
        for (j = 0; j < SIZE; j++) {
            address_c = (unsigned int)c + ((i * SIZE * sizeof(int))) + (j * sizeof(int));
            get_data((void *)address_c, &y, sizeof(int));
            printf("%d ", y);
        }
        printf("\n");
    }
    printf("Freeing the allocations!\n");
    n_free(a, ARRAY_SIZE);
    n_free(b, ARRAY_SIZE);
    n_free(c, ARRAY_SIZE);

    printf("Checking if allocations were freed!\n");
    a = n_malloc(ARRAY_SIZE);
    if ((int)a == old_a)
        printf("free function works\n");
    else
        printf("free function does not work\n");

    return 0;
}
