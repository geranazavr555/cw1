#include <cilk/cilk.h>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>

typedef int elem_type;

inline std::chrono::steady_clock::time_point now() {
    return std::chrono::steady_clock::now();
}

std::mt19937_64* rng = nullptr;
std::uniform_int_distribution<elem_type>* dist = nullptr;

elem_type* generate_array(size_t n) {
    auto *a = new elem_type[n];
    cilk_for(size_t i = 0; i < n; ++i)
        a[i] = (*dist)(*rng);
    return a;
}

void qsort_sequential(elem_type* a, size_t l, size_t r)
{
    if (l + 2 > r)
        return;

    elem_type x = a[r - 1];
    elem_type* ml = std::partition(a + l, a + r, [x](elem_type const& y) {
        return y < x;
    });
    std::swap(a[r - 1], *ml);
    elem_type* mr = ml + 1;

    qsort_sequential(a, l, (ml - a));
    qsort_sequential(a, (mr - a), r);
}

size_t block_size = 1000;
void qsort_parallel(elem_type* a, size_t l, size_t r)
{
    if (l + 2 > r)
        return;

    if (r - l < block_size) {
        qsort_sequential(a, l, r);
        return;
    }

    elem_type x = a[r - 1];
    elem_type* ml = std::partition(a + l, a + r, [x](elem_type const& y) {
        return y < x;
    });
    std::swap(a[r - 1], *ml);
    elem_type* mr = ml + 1;

    cilk_spawn qsort_parallel(a, l, (ml - a));
    qsort_parallel(a, (mr - a), r);
    cilk_sync;
}

void qsort_std(elem_type* a, size_t l, size_t r) {
    std::sort(a + l, a + r);
}

const size_t N[] = {1000, 10 * 1000, 100 * 1000, 1000 * 1000, 10 * 1000 * 1000, 100 * 1000 * 1000};
const int ATTEMPTS = 10;

std::pair<uint64_t, elem_type*> bench_micros(elem_type* a, size_t n, const std::function<void(elem_type*, size_t, size_t)>& qsort) {
    auto* a_copy = new elem_type[n];

    cilk_for(size_t i = 0; i < n; ++i)
        a_copy[i] = a[i];

    auto start = now();
    qsort(a_copy, 0, n);
    auto finish = now();

    return {
        std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count(),
        a_copy
    };
}

int main() {
    rng = new std::mt19937_64(now().time_since_epoch().count());
    dist = new std::uniform_int_distribution<elem_type>(std::numeric_limits<elem_type>::min(), std::numeric_limits<elem_type>::max());

    for (size_t n : N) {
        std::cout << "Current size: " << n << std::endl;

        uint64_t par_micros_sum = 0;
        uint64_t seq_micros_sum = 0;
        uint64_t std_micros_sum = 0;

        for (int j = 0; j < ATTEMPTS; ++j) {
            elem_type *a = generate_array(n);

            auto [std_micros, std_array] = bench_micros(a, n, qsort_std);
            std_micros_sum += std_micros;

            auto [seq_micros, seq_array] = bench_micros(a, n, qsort_sequential);
            seq_micros_sum += seq_micros;

            auto [par_micros, par_array] = bench_micros(a, n, qsort_parallel);
            par_micros_sum += par_micros;

            cilk_for(size_t k = 0; k < n; ++k)
                if (seq_array[k] != std_array[k]) {
                    std::cout << "ERROR seq in position " << k << std::endl;
                    exit(1);
                }

            cilk_for(size_t k = 0; k < n; ++k)
                if (par_array[k] != std_array[k]) {
                    std::cout << "ERROR par in position " << k << std::endl;
                    exit(1);
                }

            delete[] std_array;
            delete[] par_array;
            delete[] seq_array;
            delete[] a;
        }

        std::cout << "       std micros: " << (std_micros_sum / ATTEMPTS) << std::endl;
        std::cout << "sequential micros: " << (seq_micros_sum / ATTEMPTS) << std::endl;
        std::cout << "  parallel micros: " << (par_micros_sum / ATTEMPTS) << std::endl;
        std::cout << "Boost: " << std::setprecision(6)
                  << static_cast<double>(seq_micros_sum) / static_cast<double>(par_micros_sum) << std::endl;
    }

    delete rng;
    delete dist;
}