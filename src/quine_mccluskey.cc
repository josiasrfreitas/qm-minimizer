#include "qm/minimizer.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <iterator>
#include <map>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qm {

namespace {

using clock_t = std::chrono::steady_clock;

inline double elapsed_ms(clock_t::time_point t0) {
    using namespace std::chrono;
    return duration<double, std::milli>(clock_t::now() - t0).count();
}

inline int resolve_threads(int requested) {
    if (requested > 0) return requested;
    const unsigned hc = std::thread::hardware_concurrency();
    return hc > 0 ? static_cast<int>(hc) : 1;
}

struct GroupPair {
    std::size_t weight_a;
    std::size_t weight_b;
};

struct PairResult {
    std::vector<std::size_t> used_indices;
    std::vector<Term> fused;
};

PairResult fuse_pair(const std::vector<Term>& pool,
                     const std::vector<std::size_t>& idx_a,
                     const std::vector<std::size_t>& idx_b) {
    PairResult r;
    r.used_indices.reserve((idx_a.size() + idx_b.size()) / 2);
    r.fused.reserve(std::min(idx_a.size(), idx_b.size()));
    for (auto i : idx_a) {
        const auto& t1 = pool[i];
        for (auto j : idx_b) {
            const auto& t2 = pool[j];
            if (!can_fuse(t1, t2)) continue;
            r.fused.push_back(fuse(t1, t2));
            r.used_indices.push_back(i);
            r.used_indices.push_back(j);
        }
    }
    return r;
}

std::vector<Term> generate_primes(const Function& f,
                                  const MinimizerOptions& opts,
                                  Stats& stats) {
    std::vector<Term> pool;
    pool.reserve(f.on_set.size() + f.dc_set.size());
    for (const auto& t : f.on_set) pool.push_back(t);
    for (const auto& t : f.dc_set) pool.push_back(t);

    {
        std::unordered_set<Term, TermHash> uniq(pool.begin(), pool.end());
        pool.assign(uniq.begin(), uniq.end());
    }

    std::vector<Term> primes;
    const int n_threads = resolve_threads(opts.threads);
    stats.threads_used = n_threads;

    while (!pool.empty()) {
        std::map<std::size_t, std::vector<std::size_t>> groups;
        for (std::size_t i = 0; i < pool.size(); ++i) {
            groups[pool[i].weight()].push_back(i);
        }

        std::vector<GroupPair> work_units;
        work_units.reserve(groups.size());
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            auto nxt = std::next(it);
            if (nxt == groups.end()) break;
            if (nxt->first == it->first + 1) {
                work_units.push_back({it->first, nxt->first});
            }
        }

        std::vector<bool> used(pool.size(), false);
        std::unordered_set<Term, TermHash> next_uniq;

        auto merge = [&](PairResult&& r) {
            for (auto idx : r.used_indices) used[idx] = true;
            for (auto& t : r.fused) next_uniq.insert(std::move(t));
        };

        if (n_threads <= 1 || work_units.size() <= 1) {
            for (const auto& wu : work_units) {
                merge(fuse_pair(pool, groups[wu.weight_a],
                                groups[wu.weight_b]));
            }
        } else {
            const std::size_t parallelism =
                std::min<std::size_t>(static_cast<std::size_t>(n_threads),
                                      work_units.size());
            std::vector<std::future<PairResult>> futures;
            futures.reserve(work_units.size());
            for (const auto& wu : work_units) {
                futures.push_back(std::async(
                    std::launch::async,
                    [&pool, &groups, wu]() {
                        return fuse_pair(pool, groups[wu.weight_a],
                                         groups[wu.weight_b]);
                    }));
            }
            for (auto& fut : futures) merge(fut.get());
            (void)parallelism;
        }

        for (std::size_t i = 0; i < pool.size(); ++i) {
            if (!used[i]) primes.push_back(std::move(pool[i]));
        }

        pool.assign(std::make_move_iterator(next_uniq.begin()),
                    std::make_move_iterator(next_uniq.end()));
    }

    {
        std::unordered_set<Term, TermHash> uniq(
            std::make_move_iterator(primes.begin()),
            std::make_move_iterator(primes.end()));
        primes.assign(std::make_move_iterator(uniq.begin()),
                      std::make_move_iterator(uniq.end()));
    }

    return primes;
}

std::vector<Term> solve_cover(const std::vector<Term>& on_set,
                              const std::vector<Term>& primes,
                              Stats& stats) {
    std::vector<Term> selected;
    if (on_set.empty() || primes.empty()) return selected;

    const std::size_t M = on_set.size();
    const std::size_t P = primes.size();

    std::vector<BitVec> covers_of(P, BitVec(M));
    for (std::size_t p = 0; p < P; ++p) {
        for (std::size_t m = 0; m < M; ++m) {
            if (covers(primes[p], on_set[m])) covers_of[p].set(m);
        }
    }

    BitVec covered(M);
    std::vector<bool> picked(P, false);

    // Essentials.
    for (std::size_t m = 0; m < M; ++m) {
        if (covered.test(m)) continue;
        std::size_t which = static_cast<std::size_t>(-1);
        int count = 0;
        for (std::size_t p = 0; p < P; ++p) {
            if (covers_of[p].test(m)) {
                which = p;
                if (++count > 1) break;
            }
        }
        if (count == 1 && !picked[which]) {
            selected.push_back(primes[which]);
            picked[which] = true;
            covered |= covers_of[which];
        }
    }
    stats.essential_primes = selected.size();

    // Greedy on the remainder.
    while (covered.popcount() < M) {
        std::size_t best = static_cast<std::size_t>(-1);
        std::size_t best_count = 0;
        const BitVec uncovered = ~covered;
        for (std::size_t p = 0; p < P; ++p) {
            if (picked[p]) continue;
            BitVec gain = covers_of[p];
            gain &= uncovered;
            const std::size_t c = gain.popcount();
            if (c > best_count) {
                best_count = c;
                best = p;
            }
        }
        if (best == static_cast<std::size_t>(-1) || best_count == 0) break;
        selected.push_back(primes[best]);
        picked[best] = true;
        covered |= covers_of[best];
    }

    return selected;
}

std::size_t literals_total(const std::vector<Term>& terms) {
    std::size_t total = 0;
    for (const auto& t : terms) total += t.literal_count();
    return total;
}

}  // namespace

Solution QuineMcCluskey::minimize(const Function& f,
                                  const MinimizerOptions& opts) {
    Solution sol;
    const auto t0 = clock_t::now();

    sol.stats.input_on_terms = f.on_set.size();
    sol.stats.input_off_terms = f.off_set.size();
    sol.stats.input_dc_terms = f.dc_set.size();
    sol.stats.total_literals_in = literals_total(f.on_set);

    const auto t_a = clock_t::now();
    auto primes = generate_primes(f, opts, sol.stats);
    sol.stats.phase_a_ms = elapsed_ms(t_a);
    sol.stats.generated_primes = primes.size();

    const auto t_b = clock_t::now();
    sol.selected = solve_cover(f.on_set, primes, sol.stats);
    sol.stats.phase_b_ms = elapsed_ms(t_b);
    sol.stats.selected_primes = sol.selected.size();
    sol.stats.total_literals_out = literals_total(sol.selected);

    sol.stats.total_ms = elapsed_ms(t0);
    return sol;
}

}  // namespace qm
