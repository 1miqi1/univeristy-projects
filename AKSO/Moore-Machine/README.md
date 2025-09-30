
# Moore Automata – C Library

## Project Description

This project implements a dynamically loadable C library that simulates **Moore automata**. A Moore automaton is a type of deterministic finite automaton (DFA) commonly used in synchronous digital circuits. A Moore automaton is formally represented as a 6-tuple ⟨X, Y, Q, t, y, q⟩, where:

* **X** – set of input signal values,
* **Y** – set of output signal values,
* **Q** – set of internal states,
* **t: X × Q → Q** – transition function,
* **y: Q → Y** – output function,
* **q ∈ Q** – initial state.

This library supports **binary automata** with:

* `n` 1-bit input signals,
* `m` 1-bit output signals,
* internal state represented by `s` bits.

Formally: X = {0,1}ⁿ, Y = {0,1}ᵐ, Q = {0,1}ˢ.

At each step, the transition function `t` computes a new state based on the current state and input signals. The output function `y` computes output signals based on the current state.

---

## Library Interface

The library interface is defined in **`ma.h`**. Its usage is demonstrated in **`ma_example.c`**, which is considered part of the specification.

### Data Representation

* Bits are stored in `uint64_t` arrays.
* Each `uint64_t` element stores 64 consecutive bits starting from the least significant bit.
* Any unused bits in the last element remain uninitialized.

### Key Types

```c
typedef struct moore moore_t;

typedef void (*transition_function_t)(uint64_t *next_state, uint64_t const *input, uint64_t const *state, size_t n, size_t s);
typedef void (*output_function_t)(uint64_t *output, uint64_t const *state, size_t m, size_t s);
```

---

## Core Functions

### Creating Automata

```c
moore_t *ma_create_full(size_t n, size_t m, size_t s, transition_function_t t, output_function_t y, uint64_t const *q);
moore_t *ma_create_simple(size_t n, size_t s, transition_function_t t);
```

* `ma_create_full` creates an automaton with a custom output function and initial state.
* `ma_create_simple` creates an automaton where the output equals the state and the initial state is zeroed.

### Deleting Automata

```c
void ma_delete(moore_t *a);
```

* Frees all memory associated with the automaton. Safe to call with `NULL`.

### Connecting and Disconnecting Automata

```c
int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num);
int ma_disconnect(moore_t *a_in, size_t in, size_t num);
```

* Connects output signals of one automaton to input signals of another.
* Disconnects previously connected signals.

### Setting Inputs and State

```c
int ma_set_input(moore_t *a, uint64_t const *input);
int ma_set_state(moore_t *a, uint64_t const *state);
```

* `ma_set_input` sets values of unconnected input signals.
* `ma_set_state` sets the current state of the automaton.

### Reading Output

```c
uint64_t const *ma_get_output(moore_t const *a);
```

* Returns a pointer to the current output signals.

### Advancing Automata

```c
int ma_step(moore_t *at[], size_t num);
```

* Advances multiple automata by one synchronous step.
* Updates all states and outputs simultaneously.

---

## Functional Requirements

* Inputs can be set directly or via connections from other automata.
* Disconnected inputs are considered **undefined**.
* Proper memory management is required; no memory leaks or inconsistent states are allowed.
* Weak guarantee against allocation failures: library must remain in a valid state even on memory allocation errors.

---

## Compilation
* Compile with `gcc` using flags:

  ```text
  -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
  ```
* Link shared library with wrapped memory functions for testing:

  ```text
  -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup
  ```
---


## Provided Files

* `ma.h` – library interface (do **not** modify)
* `ma_example.c` – usage examples
* `memory_tests.c` – allocation-failure testing
* `memory_tests.h` – interface for memory tests

