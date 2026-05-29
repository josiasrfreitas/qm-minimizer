#include "qm/writer.hpp"

#include <iomanip>
#include <sstream>

namespace qm {

void PlaWriter::write(std::ostream& out,
                      const Function& source,
                      const Solution& result,
                      const WriterOptions& opts) {
    if (opts.include_header_comments) {
        out << "# qm-minimizer output\n";
        out << "# input ON terms : " << result.stats.input_on_terms << "\n";
        out << "# primes generated: " << result.stats.generated_primes << "\n";
        out << "# primes selected : " << result.stats.selected_primes
            << " (essentials: " << result.stats.essential_primes << ")\n";
        out << "# total time      : " << std::fixed << std::setprecision(3)
            << result.stats.total_ms << " ms\n";
    }

    out << ".i " << source.n_inputs << "\n";
    out << ".o " << source.n_outputs << "\n";

    if (opts.preserve_names && !source.input_names.empty()) {
        out << ".ilb";
        for (const auto& name : source.input_names) out << " " << name;
        out << "\n";
    }
    if (opts.preserve_names && !source.output_names.empty()) {
        out << ".ob";
        for (const auto& name : source.output_names) out << " " << name;
        out << "\n";
    }

    out << ".p " << result.selected.size() << "\n";
    out << ".type f\n";

    for (const auto& t : result.selected) {
        out << t.to_string() << " 1\n";
    }

    out << ".e\n";
}

}  // namespace qm
