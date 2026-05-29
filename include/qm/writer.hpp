#pragma once

#include "qm/function.hpp"
#include "qm/solution.hpp"

#include <ostream>

namespace qm {

struct WriterOptions {
    bool include_header_comments = true;
    bool preserve_names = true;
};

// Emits a Solution to an external representation.
class Writer {
public:
    virtual ~Writer() = default;
    virtual void write(std::ostream& out,
                       const Function& source,
                       const Solution& result,
                       const WriterOptions& opts = {}) = 0;
};

// Writes the minimized cover as an Espresso PLA file (`.type f`).
class PlaWriter final : public Writer {
public:
    void write(std::ostream& out,
               const Function& source,
               const Solution& result,
               const WriterOptions& opts = {}) override;
};

}  // namespace qm
