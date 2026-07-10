#include "qm/bitvec.hpp"

#include <algorithm>
#include <stdexcept>

namespace qm {

namespace {

inline std::uint64_t tail_mask(std::size_t n_bits) {
    std::size_t r = n_bits % BitVec::bits_per_word;
    if (r == 0) return ~std::uint64_t{0};
    return (std::uint64_t{1} << r) - 1;
}

}  // namespace

BitVec::BitVec(std::size_t n_bits)
    : words_(words_for(n_bits), 0), n_bits_(n_bits) {}

void BitVec::resize(std::size_t n_bits) {
    words_.assign(words_for(n_bits), 0);
    n_bits_ = n_bits;
}

void BitVec::set(std::size_t i, bool value) {
    if (value) {
        words_[i / bits_per_word] |= (std::uint64_t{1} << (i % bits_per_word));
    } else {
        reset(i);
    }
}

void BitVec::reset(std::size_t i) {
    words_[i / bits_per_word] &= ~(std::uint64_t{1} << (i % bits_per_word));
}

bool BitVec::test(std::size_t i) const {
    return (words_[i / bits_per_word] >> (i % bits_per_word)) & 1ULL;
}

void BitVec::set_all() {
    std::fill(words_.begin(), words_.end(), ~std::uint64_t{0});
    mask_tail();
}

void BitVec::reset_all() {
    std::fill(words_.begin(), words_.end(), std::uint64_t{0});
}

std::size_t BitVec::popcount() const noexcept {
    std::size_t c = 0;
    for (auto w : words_) c += static_cast<std::size_t>(__builtin_popcountll(w));
    return c;
}

bool BitVec::any() const noexcept {
    for (auto w : words_) {
        if (w) return true;
    }
    return false;
}

BitVec& BitVec::operator&=(const BitVec& other) {
    const std::size_t n = std::min(words_.size(), other.words_.size());
    for (std::size_t i = 0; i < n; ++i) words_[i] &= other.words_[i];
    return *this;
}

BitVec& BitVec::operator|=(const BitVec& other) {
    const std::size_t n = std::min(words_.size(), other.words_.size());
    for (std::size_t i = 0; i < n; ++i) words_[i] |= other.words_[i];
    mask_tail();
    return *this;
}

BitVec& BitVec::operator^=(const BitVec& other) {
    const std::size_t n = std::min(words_.size(), other.words_.size());
    for (std::size_t i = 0; i < n; ++i) words_[i] ^= other.words_[i];
    mask_tail();
    return *this;
}

BitVec BitVec::operator~() const {
    BitVec r(n_bits_);
    for (std::size_t i = 0; i < words_.size(); ++i) r.words_[i] = ~words_[i];
    r.mask_tail();
    return r;
}

bool BitVec::operator==(const BitVec& other) const noexcept {
    if (n_bits_ != other.n_bits_) return false;
    return words_ == other.words_;
}

void BitVec::mask_tail() {
    if (words_.empty()) return;
    words_.back() &= tail_mask(n_bits_);
}

std::size_t BitVecHash::operator()(const BitVec& bv) const noexcept {
    // FNV-1a 64-bit, folded into size_t.
    constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
    constexpr std::uint64_t fnv_prime  = 1099511628211ULL;
    std::uint64_t h = fnv_offset;
    for (auto w : bv.words()) {
        h ^= w;
        h *= fnv_prime;
    }
    return static_cast<std::size_t>(h);
}

}  // namespace qm
