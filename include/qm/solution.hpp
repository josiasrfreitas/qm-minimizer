#pragma once

#include "qm/stats.hpp"
#include "qm/term.hpp"

#include <vector>

namespace qm {

// Output of a Minimizer: the chosen primes (the minimized SOP) plus
// performance and structural statistics about the run.
struct Solution {
    std::vector<Term> selected;
    Stats stats;
};

}  // namespace qm
