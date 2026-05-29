#pragma once

#include "qm/bitvec.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace qm {

// A product term in Sum-of-Products form.
//
// Encoding (value, mask) per input position i:
//   mask[i] = 1, value[i] = 0  -> literal !x_i
//   mask[i] = 1, value[i] = 1  -> literal  x_i
//   mask[i] = 0                -> variable absent (don't-care)
//
// Invariant: value & ~mask == 0 (bits outside the mask are always zero).
struct Term {
    BitVec value;
    BitVec mask;

    Term() = default;
    Term(BitVec v, BitVec m) : value(std::move(v)), mask(std::move(m)) {}

    std::size_t n_inputs() const noexcept { return value.size(); }

    // Hamming weight: count of variables forced to 1.
    // Used to bucket terms before pairwise fusion.
    std::size_t weight() const noexcept { return (value & mask).popcount(); }

    // Number of literals (variables present in the term).
    std::size_t literal_count() const noexcept { return mask.popcount(); }

    bool operator==(const Term& other) const noexcept {
        return value == other.value && mask == other.mask;
    }
    bool operator!=(const Term& other) const noexcept { return !(*this == other); }

    // Render as a fixed-width PLA-style string ('0', '1', '-' per variable).
    std::string to_string() const;
};

struct TermHash {
    std::size_t operator()(const Term& t) const noexcept;
};

// True iff t1 and t2 can be merged into a single term that drops one literal.
// Requirement: same mask, value differs in exactly one bit within that mask.
bool can_fuse(const Term& t1, const Term& t2) noexcept;

// Fuses two mergeable terms. Precondition: can_fuse(t1, t2) == true.
Term fuse(const Term& t1, const Term& t2);

// True iff `prime` covers `minterm` (i.e., every literal in `prime` is
// satisfied by the input combination represented by `minterm`).
// Equivalent to: (prime.value ^ minterm.value) & prime.mask == 0.
bool covers(const Term& prime, const Term& minterm) noexcept;

}  // namespace qm
