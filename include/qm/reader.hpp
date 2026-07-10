#pragma once

#include "qm/function.hpp"

#include <istream>
#include <stdexcept>
#include <string>

namespace qm {

class ParseError : public std::runtime_error {
public:
    ParseError(std::size_t line, std::string message);
    std::size_t line() const noexcept { return line_; }

private:
    std::size_t line_;
};

// Reads a Boolean function from some external representation.
class Reader {
public:
    virtual ~Reader() = default;
    virtual Function read(std::istream& in) = 0;
};

// Reads functions in the Espresso PLA dialect (single-output subset).
// Recognized directives: .i .o .p .type .ilb .ob .e .end
class PlaReader final : public Reader {
public:
    Function read(std::istream& in) override;
};

}  // namespace qm
