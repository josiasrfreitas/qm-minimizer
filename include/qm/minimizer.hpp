#pragma once

#include "qm/function.hpp"
#include "qm/solution.hpp"

#include <chrono>

namespace qm {

struct MinimizerOptions {
    // 0 means use std::thread::hardware_concurrency().
    int threads = 0;

    // Cover-phase strategy. Greedy is the current default; B&B is reserved
    // for a future revision (see ADR 0005).
    enum class CoverStrategy { Greedy };
    CoverStrategy cover = CoverStrategy::Greedy;

    // Soft time limit. Zero disables.
    std::chrono::milliseconds time_limit{0};
};

// Logic minimizer interface.
class Minimizer {
public:
    virtual ~Minimizer() = default;
    virtual Solution minimize(const Function& f, const MinimizerOptions& opts = {}) = 0;
};

// Quine-McCluskey: exhaustive prime-implicant generation followed by a
// heuristic cover (essentials + greedy).
class QuineMcCluskey final : public Minimizer {
public:
    Solution minimize(const Function& f, const MinimizerOptions& opts = {}) override;
};

}  // namespace qm
