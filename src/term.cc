#include "qm/term.hpp"

namespace qm {

std::string Term::to_string() const {
    std::string s(n_inputs(), '-');
    for (std::size_t i = 0; i < n_inputs(); ++i) {
        if (mask.test(i)) {
            s[i] = value.test(i) ? '1' : '0';
        }
    }
    return s;
}

std::size_t TermHash::operator()(const Term& t) const noexcept {
    BitVecHash bvh;
    const std::size_t h1 = bvh(t.value);
    const std::size_t h2 = bvh(t.mask);
    return h1 ^ (h2 * 0x9E3779B97F4A7C15ULL);
}

bool can_fuse(const Term& t1, const Term& t2) noexcept {
    if (t1.mask != t2.mask) return false;
    BitVec diff = t1.value ^ t2.value;
    diff &= t1.mask;
    return diff.popcount() == 1;
}

Term fuse(const Term& t1, const Term& t2) {
    BitVec diff = t1.value ^ t2.value;
    diff &= t1.mask;
    Term out;
    out.mask = t1.mask;
    out.mask ^= diff;  // drop the differing literal
    out.value = t1.value;
    out.value &= out.mask;  // preserve invariant: bits outside mask are 0
    return out;
}

bool covers(const Term& prime, const Term& minterm) noexcept {
    BitVec d = prime.value ^ minterm.value;
    d &= prime.mask;
    return d.none();
}

}  // namespace qm
