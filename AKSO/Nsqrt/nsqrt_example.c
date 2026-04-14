#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// To jest deklaracja testowanej funkcji.
void nsqrt(uint64_t *Q, uint64_t *X, unsigned n);

#define SIZE(x) (sizeof x / sizeof x[0])

typedef __uint128_t uint128_t;

// To jest struktura przechowująca dane do testów.
typedef union {
  uint64_t  u64[2];
  uint128_t u128;
} test_data_t;

// Tu są dane do testów.
static const test_data_t test_data_table[] = {
  {{0, 0}},
  {{1, 0}},
  {{2, 0}},
  {{3, 0}},
  {{4, 0}},
  {{5, 0}},
  {{UINT64_MAX - 1, 0}},
  {{UINT64_MAX, 0}},
  {{0, 1}},
  {{1, 1}},
  {{UINT64_MAX - 1, UINT64_MAX}},
  {{UINT64_MAX, UINT64_MAX}}
};

int main() {
  uint64_t *Q, *X;
  Q = malloc(sizeof (uint64_t));
  X = malloc(2 * sizeof (uint64_t));
  assert(Q && X);
  for (unsigned i = 0; i < SIZE(test_data_table); ++i) {
    memcpy(X, &test_data_table[i], 2 * sizeof (uint64_t));
    nsqrt(Q, X, 8 * sizeof (uint64_t));
    printf("Q = %" PRIu64 "\n", Q[0]);
    uint128_t Q1 = Q[0];
    uint128_t Q2 = Q1 * Q1;
    assert(Q2 <= test_data_table[i].u128 && Q2 + Q1 >= test_data_table[i].u128 - Q1);
  }
  free(Q);
  free(X);
}
