/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// -*- c++ -*-

#include <faiss/utils/random.h>

extern "C" {
int sgemm_(
        const char* transa,
        const char* transb,
        FINTEGER* m,
        FINTEGER* n,
        FINTEGER* k,
        const float* alpha,
        const float* a,
        FINTEGER* lda,
        const float* b,
        FINTEGER* ldb,
        float* beta,
        float* c,
        FINTEGER* ldc);
}

namespace faiss {

/**************************************************
 * Random data generation functions
 **************************************************/

RandomGenerator::RandomGenerator(int64_t seed) : mt((unsigned int)seed) {}

int RandomGenerator::rand_int() {
    return mt() & 0x7fffffff;
}

int64_t RandomGenerator::rand_int64() {
    return int64_t(rand_int()) | int64_t(rand_int()) << 31;
}

int RandomGenerator::rand_int(int max) {
    return mt() % max;
}

float RandomGenerator::rand_float() {
    return mt() / float(mt.max());
}

double RandomGenerator::rand_double() {
    return mt() / double(mt.max());
}

SplitMix64RandomGenerator::SplitMix64RandomGenerator(int64_t seed)
        : state{static_cast<uint64_t>(seed)} {}

int SplitMix64RandomGenerator::rand_int() {
    return next() & 0x7fffffff;
}

int64_t SplitMix64RandomGenerator::rand_int64() {
    uint64_t value = next();
    return static_cast<int64_t>(value & 0x7fffffffffffffffULL);
}

int SplitMix64RandomGenerator::rand_int(int max) {
    return next() % max;
}

float SplitMix64RandomGenerator::rand_float() {
    return next() / float(std::numeric_limits<uint64_t>::max());
}

double SplitMix64RandomGenerator::rand_double() {
    return next() / double(std::numeric_limits<uint64_t>::max());
}

uint64_t SplitMix64RandomGenerator::next() {
    uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/***********************************************************************
 * Random functions in this C file only exist because Torch
 *  counterparts are slow and not multi-threaded.  Typical use is for
 *  more than 1-100 billion values. */

/* Generate a set of random floating point values such that x[i] in [0,1]
   multi-threading. For this reason, we rely on re-entreant functions.  */
void float_rand(float* x, size_t n, int64_t seed) {
    // only try to parallelize on large enough arrays
    const size_t nblock = n < 1024 ? 1 : 1024;

    RandomGenerator rng0(seed);
    int a0 = rng0.rand_int(), b0 = rng0.rand_int();

#pragma omp parallel for
    for (int64_t j = 0; j < nblock; j++) {
        RandomGenerator rng(a0 + j * b0);

        const size_t istart = j * n / nblock;
        const size_t iend = (j + 1) * n / nblock;

        for (size_t i = istart; i < iend; i++) {
            x[i] = rng.rand_float();
        }
    }
}

void float_randn(float* x, size_t n, int64_t seed) {
    // only try to parallelize on large enough arrays
    const size_t nblock = n < 1024 ? 1 : 1024;

    RandomGenerator rng0(seed);
    int a0 = rng0.rand_int(), b0 = rng0.rand_int();

#pragma omp parallel for
    for (int64_t j = 0; j < nblock; j++) {
        RandomGenerator rng(a0 + j * b0);

        double a = 0, b = 0, s = 0;
        int state = 0; /* generate two number per "do-while" loop */

        const size_t istart = j * n / nblock;
        const size_t iend = (j + 1) * n / nblock;

        for (size_t i = istart; i < iend; i++) {
            /* Marsaglia's method (see Knuth) */
            if (state == 0) {
                do {
                    a = 2.0 * rng.rand_double() - 1;
                    b = 2.0 * rng.rand_double() - 1;
                    s = a * a + b * b;
                } while (s >= 1.0);
                x[i] = a * sqrt(-2.0 * log(s) / s);
            } else {
                x[i] = b * sqrt(-2.0 * log(s) / s);
            }
            state = 1 - state;
        }
    }
}

/* Integer versions */
void int64_rand(int64_t* x, size_t n, int64_t seed) {
    // only try to parallelize on large enough arrays
    const size_t nblock = n < 1024 ? 1 : 1024;

    RandomGenerator rng0(seed);
    int a0 = rng0.rand_int(), b0 = rng0.rand_int();

#pragma omp parallel for
    for (int64_t j = 0; j < nblock; j++) {
        RandomGenerator rng(a0 + j * b0);

        const size_t istart = j * n / nblock;
        const size_t iend = (j + 1) * n / nblock;
        for (size_t i = istart; i < iend; i++) {
            x[i] = rng.rand_int64();
        }
    }
}

void int64_rand_max(int64_t* x, size_t n, uint64_t max, int64_t seed) {
    // only try to parallelize on large enough arrays
    const size_t nblock = n < 1024 ? 1 : 1024;

    RandomGenerator rng0(seed);
    int a0 = rng0.rand_int(), b0 = rng0.rand_int();

#pragma omp parallel for
    for (int64_t j = 0; j < nblock; j++) {
        RandomGenerator rng(a0 + j * b0);

        const size_t istart = j * n / nblock;
        const size_t iend = (j + 1) * n / nblock;
        for (size_t i = istart; i < iend; i++) {
            x[i] = rng.rand_int64() % max;
        }
    }
}

void rand_perm(int* perm, size_t n, int64_t seed) {
    for (size_t i = 0; i < n; i++) {
        perm[i] = i;
    }

    RandomGenerator rng(seed);

    for (size_t i = 0; i + 1 < n; i++) {
        int i2 = i + rng.rand_int(n - i);
        std::swap(perm[i], perm[i2]);
    }
}

void rand_perm_splitmix64(int* perm, size_t n, int64_t seed) {
    for (size_t i = 0; i < n; i++) {
        perm[i] = i;
    }

    SplitMix64RandomGenerator rng(seed);

    for (size_t i = 0; i + 1 < n; i++) {
        int i2 = i + rng.rand_int(n - i);
        std::swap(perm[i], perm[i2]);
    }
}

void byte_rand(uint8_t* x, size_t n, int64_t seed) {
    // only try to parallelize on large enough arrays
    const size_t nblock = n < 1024 ? 1 : 1024;

    RandomGenerator rng0(seed);
    int a0 = rng0.rand_int(), b0 = rng0.rand_int();

#pragma omp parallel for
    for (int64_t j = 0; j < nblock; j++) {
        RandomGenerator rng(a0 + j * b0);

        const size_t istart = j * n / nblock;
        const size_t iend = (j + 1) * n / nblock;

        size_t i;
        for (i = istart; i < iend; i++) {
            x[i] = rng.rand_int64();
        }
    }
}

void rand_smooth_vectors(size_t n, size_t d, float* x, int64_t seed) {
    size_t d1 = 10;
    std::vector<float> x1(n * d1);
    float_randn(x1.data(), x1.size(), seed);
    std::vector<float> rot(d1 * d);
    float_rand(rot.data(), rot.size(), seed + 1);

    { //
        FINTEGER di = d, d1i = d1, ni = n;
        float one = 1.0, zero = 0.0;
        sgemm_("Not transposed",
               "Not transposed", // natural order
               &di,
               &ni,
               &d1i,
               &one,
               rot.data(),
               &di, // rotation matrix
               x1.data(),
               &d1i, // second term
               &zero,
               x,
               &di);
    }

    std::vector<float> scales(d);
    float_rand(scales.data(), d, seed + 2);

#pragma omp parallel for if (n * d > 10000)
    for (int64_t i = 0; i < n; i++) {
        for (size_t j = 0; j < d; j++) {
            x[i * d + j] = sinf(x[i * d + j] * (scales[j] * 4 + 0.1));
        }
    }
}

} // namespace faiss
