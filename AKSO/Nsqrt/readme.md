
# Integer Square Root

## Project Description

The goal of this project is to implement an **assembly function** that computes the integer square root of a non-negative integer represented in binary form.

Given a **2n-bit non-negative integer** `X`, we want to find a non-negative **n-bit integer** `Q` such that:

```
Q² ≤ X < (Q + 1)²
```

## Function Specification

The function should be callable from C with the following declaration:

```c
void nsqrt(uint64_t *Q, uint64_t *X, unsigned n);
```

* `Q` – pointer to the result Q in binary form (little-endian, 64-bit words).
* `X` – pointer to the input number X in binary form (little-endian, 64-bit words). Memory may be modified for workspace purposes.
* `n` – number of bits in the result, divisible by 64, in the range 64–256000.

Numbers are represented in **natural binary code**, little-endian, in 64-bit words (`uint64_t`).

## Suggested Algorithm

The result is calculated iteratively:

1. Let `Qj = Σ (qi * 2^(n-i))` – the result after `j` iterations, where `qi ∈ {0,1}`.
2. Let `Rj` – the remainder after `j` iterations. Start with `Q0 = 0` and `R0 = X`.
3. In iteration `j`, determine the bit `qj` of the result:

   * `Tj-1 = 2^(n-j+1) * Qj-1 + 4^(n-j)`
   * If `Rj-1 ≥ Tj-1`, set `qj = 1` and `Rj = Rj-1 - Tj-1`.
   * Otherwise, set `qj = 0` and `Rj = Rj-1`.

After `n` iterations:

```
Rn = X - Q²
```

It can be proven that:

```
0 ≤ Rn ≤ 2Q
```

## Project Files

* `nsqrt.asm` – assembly implementation of the function.
* `nsqrt_example.c` – example usage in C.
* `nsqrt_example.cpp` – example usage in C++.

## Compilation

### Assembly

```bash
nasm -f elf64 -w+all -w+error -o nsqrt.o nsqrt.asm
```

### Compile C Example

```bash
gcc -c -Wall -Wextra -std=c17 -O2 -o nsqrt_example_64.o nsqrt_example.c
gcc -z noexecstack -o nsqrt_example_64 nsqrt_example_64.o nsqrt.o
```

### Compile C++ Example

```bash
g++ -c -Wall -Wextra -std=c++20 -O2 -o nsqrt_example_cpp.o nsqrt_example.cpp
g++ -z noexecstack -o nsqrt_example_cpp nsqrt_example_cpp.o nsqrt.o -lgmp
```

## Requirements

* Linux x86_64
* `nasm`
* `gcc` / `g++`
* (optional: `gmp` for the C++ example)

## Notes

* The function uses memory pointed to by `X` as workspace.
* Must handle numbers up to 256,000 bits.
