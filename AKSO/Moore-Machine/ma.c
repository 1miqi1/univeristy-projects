#include "ma.h"
#include "memory_tests.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t *output_pointer; // Pointer to the uint64_t element
    size_t bit_number; // Position of the bit within the element
} bit_pointer_t;

uint64_t find_value(bit_pointer_t bit_pointer) {
    // Calculate the index of the uint64_t element containing the bit
    size_t position = bit_pointer.bit_number / 64;

    // Extract the value of the bit at the specified position
    uint64_t bit_value = bit_pointer.output_pointer[position] &  (1 << (bit_pointer.bit_number % 64));

    return bit_value;
}

struct moore {
    size_t n; 
    size_t m; 
    size_t s; 
    uint64_t *state; 
    uint64_t *input; 
    uint64_t *output; 
    transition_function_t transition; 
    output_function_t output_function; 
    bit_pointer_t *connection_of_input; //adresy podłączonych wejść:
};

moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
    output_function_t y, uint64_t const *q) {
    // Allocate memory for the moore_t structure
    if (m == 0 || s == 0 || t == NULL || y == NULL || q == NULL) {
        errno = EINVAL; // Set errno to EINVAL for invalid arguments
        return NULL;
    }

    // Allocate memory for the moore_t structure
    moore_t *machine = (moore_t *)malloc(sizeof(moore_t));
    if (!machine) {
        errno = ENOMEM; // Set errno to ENOMEM if memory allocation fails
        return NULL;
    }

    // Initialize the fields of the structure
    machine->n = n;
    machine->m = m;
    machine->s = s;
    machine->transition = t;
    machine->output_function = y;

    // Calculate the number of uint64_t elements needed to store the bits
    size_t state_size = (s + 63) / 64; // Number of uint64_t elements for `s` bits
    size_t input_size = (n + 63) / 64; // Number of uint64_t elements for `n` bits
    size_t output_size = (m + 63) / 64; // Number of uint64_t elements for `m` bits

    // Allocate memory for state, input, and output arrays
    machine->state = (uint64_t *)malloc(state_size * sizeof(uint64_t));
    machine->input = (uint64_t *)malloc(input_size * sizeof(uint64_t));
    machine->output = (uint64_t *)malloc(output_size * sizeof(uint64_t));
    machine->connection_of_input = (bit_pointer_t*)malloc(n * sizeof(bit_pointer_t));

    if (!machine->state || !machine->input || !machine->output || !machine->connection_of_input) {
        // Free allocated memory if any allocation fails
        free(machine->state);
        free(machine->input);
        free(machine->output);
        free(machine->connection_of_input);
        free(machine);
        return NULL;
    }

    // Initialize the state array with the provided initial state (q)
    memcpy(machine->state, q, state_size * sizeof(uint64_t));

    // Initialize connection_of_input to NULL
    for (size_t i = 0; i < n; i++) {
        machine->connection_of_input[i].output_pointer = NULL;
    }

    return machine;
}

void id(uint64_t *output, uint64_t const *state, size_t s, size_t) {
    memcpy(output, state, s * sizeof(uint64_t));
}

moore_t * ma_create_simple(size_t n, size_t m, transition_function_t t){
    return ma_create_full(n,m,m,t,id,0);
}

void ma_delete(moore_t *a) {
    if (!a) {
        return; // If the pointer is NULL, do nothing
    }

    // Free the dynamically allocated arrays
    free(a->state);
    free(a->input);
    free(a->output);

    // Free the connection_of_input array
    free(a->connection_of_input);

    // Free the moore_t structure itself
    free(a);
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num) {
    // Validate parameters
    if (!a_in || !a_out || num == 0 || in + num > a_in->n || out + num > a_out->m) {
        errno = EINVAL; // Set errno to EINVAL for invalid arguments
        return -1;
    }
    /* ZAKTUALIZOWAĆ INPUT*/
    // Connect the inputs to the outputs
    for (size_t i = 0; i < num; i++) {
        a_in->connection_of_input[in + i].output_pointer = a_out -> output;
        a_in->connection_of_input[in + i].bit_number = out + i;
    }

    return 0; // Return 0 on success
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num){
    if(!a_in || num == 0 || in + num > a_in->n){
        errno = EINVAL; // Set errno to EINVAL for invalid arguments
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        a_in->connection_of_input[in + i].output_pointer = NULL;
    }

    return 0; // Return 0 on success 
}

static void modify_input(moore_t *a){

}