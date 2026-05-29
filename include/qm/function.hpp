#pragma once

#include "qm/term.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace qm {

// PLA semantic type.
//   F  : only the ON-set is listed; everything else is OFF.
//   FR : ON and OFF sets are listed; everything else is don't-care.
//   FD : ON and DC sets are listed; everything else is OFF.
enum class PlaType { F, FR, FD };

// The common in-memory representation of a Boolean function, decoupled
// from any input/output format. Every Reader produces one of these; every
// Minimizer consumes one of these.
struct Function {
    std::size_t n_inputs = 0;
    std::size_t n_outputs = 0;  // current scope supports only n_outputs == 1

    PlaType type = PlaType::F;

    // Optional metadata preserved across read/write roundtrips.
    std::vector<std::string> input_names;
    std::vector<std::string> output_names;

    // ON-set: input combinations that must be covered.
    std::vector<Term> on_set;
    // OFF-set: input combinations that must NOT be covered.
    std::vector<Term> off_set;
    // DC-set: input combinations free to be covered or not. Phase A may
    // fuse with these to grow primes; Phase B does not need to cover them.
    std::vector<Term> dc_set;
};

}  // namespace qm
