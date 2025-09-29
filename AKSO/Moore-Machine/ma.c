#include "ma.h"
#include "memory_tests.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Struktura reprezentująca wskaźnik na bit wejścia/wyjścia automatu.
 */ 
typedef struct {
    moore_t *des;
    size_t bit_number;
} bit_pointer_t;

/*
 * Struktura przechowująca elementy typu 'bit_pointer', umożliwiająca ich dodawanie i usuwanie.
 */ 
typedef struct {
    bit_pointer_t* data;
    size_t size;
    size_t capacity;
} Vector;

static bool init_vector(Vector* vec) {
    vec->capacity = 2;
    vec->size = 0;
    vec->data = (bit_pointer_t*)malloc(vec->capacity * sizeof(bit_pointer_t));
    
    if (!vec->data) {
        return false;
    }
    
    return true;
}

static bool push_back(Vector* vec, bit_pointer_t value) {
    if (vec->size == vec->capacity) {
        vec->capacity *= 2;
        bit_pointer_t* new_data = (bit_pointer_t*)realloc(vec->data, vec->capacity * sizeof(bit_pointer_t));
        
        if (new_data == NULL) {
            return false;
        }
        
        vec->data = new_data;
    }
    
    vec->data[vec->size++] = value;
    return true;
}

static void pop_back(Vector* vec) {
    if (!vec || vec->size == 0) return;
    vec->size--;
}

static void erase(Vector *vec, size_t i) {
    if (!vec || i >= vec->size) return;
    
    vec->data[i].des = NULL;
    vec->data[i].bit_number = 0;

    vec->data[i] = vec->data[vec->size-1];
    vec->size--;
}

static void delete_vector(Vector* vec) {
    if (vec && vec->data) {
        for (size_t i = 0; i < vec->size; i++) {
            vec->data[i].des = NULL;
        }
        
        free(vec->data);
        vec->data = NULL;
        vec->size = 0;
        vec->capacity = 0;
    }
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
    bit_pointer_t *input_connections;  // Przechowuje adresy bitów podłączonych do wejścia
    Vector *output_connections;        // Przechowuje wektory adresów bitów podłączonych do wyjścia
};

moore_t *ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
                        output_function_t y, uint64_t const *q) {
    if (m == 0 || s == 0 || t == NULL || y == NULL || q == NULL) {
        errno = EINVAL;
        return NULL;
    }

    moore_t *machine = malloc(sizeof(moore_t));
    if (!machine) {
        errno = ENOMEM;
        return NULL;
    }

    bool allocation_fail = false;

    machine->state = NULL;
    machine->input = NULL;
    machine->output = NULL;
    machine->input_connections = NULL;
    machine->output_connections = NULL;

    size_t state_size = (s + 63) / 64; 
    size_t input_size = (n + 63) / 64; 
    size_t output_size = (m + 63) / 64; 

    if (n != 0) {
        machine->input = (uint64_t*)malloc(input_size * sizeof(uint64_t));
        machine->input_connections = (bit_pointer_t*)malloc(n * sizeof(bit_pointer_t));
        
        if (!machine->input || !machine->input_connections) {
            allocation_fail = true;
        }
    }

    machine->output_connections = (Vector*)malloc(m * sizeof(Vector));
    
    if (machine->output_connections) {
        for (size_t i = 0; i < m; i++) {
            if (!init_vector(&machine->output_connections[i])) {
                allocation_fail = true;    
            }
        }
    } 
    else {
        allocation_fail = true;
    }

    machine->state = (uint64_t*)malloc(state_size * sizeof(uint64_t));
    machine->output = (uint64_t*)malloc(output_size * sizeof(uint64_t));
    
    if (!machine->state || !machine->output || allocation_fail) {
        free(machine->state);
        free(machine->output);
        free(machine->input_connections);
        free(machine->input);
        
        if (machine->output_connections) {
            for (size_t i = 0; i < m; i++) {
                delete_vector(&machine->output_connections[i]);
            }
            free(machine->output_connections);
        }
        
        free(machine);
        errno = ENOMEM;
        return NULL;
    }

    machine->n = n;
    machine->m = m;
    machine->s = s;
    machine->transition = t;
    machine->output_function = y;

    memcpy(machine->state, q, state_size * sizeof(uint64_t));
    
    if (n != 0) {
        memset(machine->input, 0, input_size * sizeof(uint64_t));
    }
    
    memset(machine->output, 0, output_size * sizeof(uint64_t));

    for (size_t i = 0; i < n; i++) {
        machine->input_connections[i].des = NULL;
        machine->input_connections[i].bit_number = 0;
    }

    machine->output_function(machine->output, machine->state, machine->m, machine->s);

    return machine;
}

static void id(uint64_t *output, uint64_t const *state, size_t s, size_t) {
    memcpy(output, state, ((s + 63) / 64) * sizeof(uint64_t));
}

moore_t *ma_create_simple(size_t n, size_t s, transition_function_t t) {
    if (s == 0 || t == NULL) {
        errno = EINVAL;
        return NULL;
    }

    size_t state_size = (s + 63) / 64;
    uint64_t *initial_state = calloc(state_size, sizeof(uint64_t));
    
    if (!initial_state) {
        errno = ENOMEM;
        return NULL;
    }

    moore_t *machine = ma_create_full(n, s, s, t, id, initial_state);
    free(initial_state);
    
    if (!machine) {
        errno = ENOMEM;
    }
    
    return machine;
}

/*
 * Funkcja rozłączająca połączenie wchodzące do danego bitu wejścia.
 */
static void delete_connection_in(moore_t *a, size_t position) {
    bit_pointer_t bit_p = a->input_connections[position];
    
    if (!bit_p.des) {
        return;
    }

    Vector *vec = &bit_p.des->output_connections[bit_p.bit_number];
    
    for (size_t i = 0; i < vec->size; i++) {
        bit_pointer_t s = vec->data[i];

        // Sprawdzamy czy 's' odpowiada za połączenie
        if (s.des == a && s.bit_number == position) {
            erase(vec, i);
        }
    }
    
    a->input_connections[position].des = NULL;
}

void ma_delete(moore_t *a) {
    if (!a) return;

    // Usuwamy wszystkie wchodzące połączenia
    if (a->input_connections) {
        for (size_t i = 0; i < a->n; i++) {
            delete_connection_in(a, i);
        }
    }

    // Usuwamy wszystkie wychodzące połączenia
    if (a->output_connections) {
        for (size_t i = 0; i < a->m; i++) {
            Vector *vec = &a->output_connections[i];
            
            while (vec->size > 0) {
                bit_pointer_t conn = vec->data[vec->size-1];
                conn.des->input_connections[conn.bit_number].des = NULL;
                pop_back(vec);
            }
            
            delete_vector(&a->output_connections[i]);
        }
    }

    free(a->state);
    free(a->input);
    free(a->output);
    free(a->input_connections);
    free(a->output_connections);
    free(a);
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num) {
    if (!a_in || !a_out || num == 0 || in + num > a_in->n || out + num > a_out->m) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        delete_connection_in(a_in, in + i);
    }

    for (size_t i = 0; i < num; i++) {
        a_in->input_connections[in + i].des = a_out;
        a_in->input_connections[in + i].bit_number = out + i;
        
        bit_pointer_t t;
        t.des = a_in;
        t.bit_number = in + i;
    
        if (!push_back(&a_out->output_connections[out + i], t)) {
            errno = ENOMEM;
            return -1;
        }
    }
    
    return 0;
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    if (!a_in || num == 0 || in + num > a_in->n) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        delete_connection_in(a_in, in + i);
    }
    
    return 0;
}

/*
 * Funkcja aktualizująca bit o pozycji 'position' w tabeli 'input'.
 */
static void set_bit(uint64_t *input, uint64_t bit, uint64_t position) {
    if (bit == 1) {
        input[position / 64] |= (1ULL << (position % 64));
    } 
    else {
        input[position / 64] &= ~(1ULL << (position % 64));
    }
}

/*
 * Funkcja aktualizująca wejście automatu na podstawie połączeń.
 */
static void update_input(moore_t *a) {
    if (!a) return;
    
    for (size_t i = 0; i < a->n; i++) {
        bit_pointer_t bit_pointer = a->input_connections[i];
        
        if (bit_pointer.des) {
            size_t position = bit_pointer.bit_number / 64;
            uint64_t bit = (bit_pointer.des->output[position] >> (bit_pointer.bit_number % 64)) & 1;
            set_bit(a->input, bit, i);
        }
    }
}

int ma_set_input(moore_t *a, uint64_t const *input) {
    if (!a || !input || a->n == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < a->n; i++) {
        if (!a->input_connections[i].des) {
            set_bit(a->input, (input[i/64] >> (i%64)) & 1, i);
        }
    }
    
    return 0;
}

int ma_set_state(moore_t *a, uint64_t const *state) {
    if (!a || !state) {
        errno = EINVAL;
        return -1;
    }
    
    size_t state_size = (a->s + 63) / 64;
    memcpy(a->state, state, state_size * sizeof(uint64_t));
    a->output_function(a->output, a->state, a->m, a->s);
    
    return 0;
}

uint64_t const *ma_get_output(moore_t const *a) {
    if (!a || !a->output) {
        errno = EINVAL;
        return NULL;
    }
    
    return a->output;
}

int ma_step(moore_t *at[], size_t num) {
    if (!at || num == 0) {
        errno = EINVAL;
        return -1;
    }

    for (size_t i = 0; i < num; i++) {
        if (!at[i] || !at[i]->state || !at[i]->output) {
            errno = EINVAL;
            return -1;
        }
    }

    for (size_t i = 0; i < num; i++) {
        update_input(at[i]);
    }

    for (size_t i = 0; i < num; i++) {
        size_t state_size = (at[i]->s + 63) / 64;
        uint64_t *next_state = malloc(state_size * sizeof(uint64_t));
        
        if (!next_state) {
            errno = ENOMEM;
            return -1;
        }

        at[i]->transition(next_state, at[i]->input, at[i]->state, at[i]->n, at[i]->s);
        memcpy(at[i]->state, next_state, state_size * sizeof(uint64_t));
        free(next_state);
        at[i]->output_function(at[i]->output, at[i]->state, at[i]->m, at[i]->s);
    }
    
    return 0;
}
