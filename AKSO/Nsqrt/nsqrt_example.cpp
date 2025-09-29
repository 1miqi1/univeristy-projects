#include <cassert>
#include <cinttypes>
#include <chrono>
#include <iostream>
#include <string>
#include <boost/multiprecision/gmp.hpp>

// To jest deklaracja testowanej funkcji.
extern "C" void nsqrt(uint64_t *Q, uint64_t *X, unsigned n);

using std::cout;
using std::cerr;
using std::stol;
using std::chrono::system_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using uint2n_t = boost::multiprecision::mpz_int;

namespace {
  // Funkcje konwertujące są jawnie podane, aby pokazać,
  // jakiego kodowania liczb wymaga funkcja nsqrt.

  template<typename T>
  void convert2bin(T in, uint64_t *out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      out[i] = (uint64_t)(in & UINT64_MAX);
      in >>= 64;
    }
  }

  template<typename T>
  void convert2boost(T &out, uint64_t const *in, size_t n) {
    out = 0;
    for (size_t i = n; i-- > 0;) {
      out <<= 64;
      out += in[i];
    }
  }
}

int main(int argc, char *args[]) {
  if (argc != 3) {
    cerr << "Usage:\n"
         << args[0] << " n X\n"
         << "Examples:\n"
         << args[0] << " 64 0\n"
         << args[0] << " 128 1000000\n"
         << args[0] << " 256 0x12ab\n"
         << args[0] << " 640 -1\n";
    return 1;
  }

  long k = stol(args[1]);
  unsigned n = k;
  uint2n_t X(args[2]), Q;

  if (X < 0)
    X += uint2n_t(1) << 2 * n;

  assert(k % 64 == 0 && k >= 64 && k <= 256000);
  assert(X >= 0 && X < uint2n_t(1) << 2 * n);

  cout << "n = " << n << "\n"
       << "X = " << X << "\n";

  uint64_t *x = new uint64_t[n/32], *q = new uint64_t[n/64];

  convert2bin(X, x, n/32);

  auto begin = system_clock::now();

  nsqrt(q, x, n);

  auto end = system_clock::now();

  convert2boost(Q, q, n/64);

  delete[] x;
  delete[] q;

  cout << "Q = " << Q << "\n"
       << duration_cast<milliseconds>(end - begin).count() << " ms\n";

  assert(Q >= 0 && Q * Q <= X && Q * Q + Q >= X - Q);
}
