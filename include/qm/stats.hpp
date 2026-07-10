#pragma once

#include <cstddef>

namespace qm {

struct Stats {
    std::size_t input_on_terms = 0;
    std::size_t input_off_terms = 0;
    std::size_t input_dc_terms = 0;

    std::size_t generated_primes = 0;
    std::size_t essential_primes = 0;
    std::size_t selected_primes = 0;

    std::size_t total_literals_in = 0;
    std::size_t total_literals_out = 0;

    double parse_ms = 0.0;
    double phase_a_ms = 0.0;
    double phase_b_ms = 0.0;
    double emit_ms = 0.0;
    double total_ms = 0.0;

    int threads_used = 1;
};

}  // namespace qm
