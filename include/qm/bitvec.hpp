#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qm {

// Dynamic-width bit vector backed by a vector<uint64_t>.
//
// Provides the bitwise ops needed by Quine-McCluskey (AND, OR, XOR, NOT,
// popcount, equality) on operands whose width is fixed at construction
// time but unknown at compile time. For n = 512 inputs (largest case in
// the IWLS benchmark) each BitVec uses 8 words = 64 bytes; a Term is two
// BitVecs = 128 bytes.
class BitVec {
public:
    static constexpr std::size_t bits_per_word = 64;

    BitVec() = default;
    explicit BitVec(std::size_t n_bits);

    void resize(std::size_t n_bits);
    std::size_t size() const noexcept { return n_bits_; }

    void set(std::size_t i, bool value = true);
    void reset(std::size_t i);
    bool test(std::size_t i) const;

    void set_all();
    void reset_all();

    std::size_t popcount() const noexcept;
    bool any() const noexcept;
    bool none() const noexcept { return !any(); }

    BitVec& operator&=(const BitVec& other);
    BitVec& operator|=(const BitVec& other);
    BitVec& operator^=(const BitVec& other);
    BitVec operator~() const;

    friend BitVec operator&(BitVec a, const BitVec& b) { a &= b; return a; }
    friend BitVec operator|(BitVec a, const BitVec& b) { a |= b; return a; }
    friend BitVec operator^(BitVec a, const BitVec& b) { a ^= b; return a; }

    bool operator==(const BitVec& other) const noexcept;
    bool operator!=(const BitVec& other) const noexcept { return !(*this == other); }

    const std::vector<std::uint64_t>& words() const noexcept { return words_; }
    std::vector<std::uint64_t>& words() noexcept { return words_; }

    static std::size_t words_for(std::size_t n_bits) {
        return (n_bits + bits_per_word - 1) / bits_per_word;
    }

private:
    std::vector<std::uint64_t> words_;
    std::size_t n_bits_ = 0;

    void mask_tail();
};

struct BitVecHash {
    std::size_t operator()(const BitVec& bv) const noexcept;
};

}  // namespace qm
