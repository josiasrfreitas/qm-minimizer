#include "qm/minimizer.hpp"
#include "qm/reader.hpp"
#include "qm/writer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Args {
    std::string input;
    std::string output;
    std::string bench_dir;
    std::string csv;
    int threads = 0;
    bool stats = false;
    bool quiet = false;
};

void print_usage(std::ostream& out) {
    out << "qm-minimizer — Quine-McCluskey SOP minimizer\n\n"
           "Usage:\n"
           "  qm <input.pla> [-o output.pla] [--stats] [--threads N]\n"
           "  qm --bench <dir>  [--csv file.csv] [--threads N]\n\n"
           "Options:\n"
           "  -o, --output FILE   Write minimized PLA to FILE (default: stdout)\n"
           "      --stats         Print run statistics to stderr\n"
           "      --threads N     Worker threads in Phase A (default: hw concurrency)\n"
           "      --bench DIR     Minimize every .pla in DIR; report per-file stats\n"
           "      --csv FILE      With --bench: write CSV summary to FILE\n"
           "      --quiet         Suppress non-error stderr output\n"
           "  -h, --help          Show this help and exit\n";
}

[[noreturn]] void die(const std::string& msg, int code = 2) {
    std::cerr << "qm: " << msg << "\n";
    std::exit(code);
}

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need_value = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) die("flag " + flag + " requires a value");
            return argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            print_usage(std::cout);
            std::exit(0);
        } else if (arg == "-o" || arg == "--output") {
            a.output = need_value(arg);
        } else if (arg == "--stats") {
            a.stats = true;
        } else if (arg == "--quiet") {
            a.quiet = true;
        } else if (arg == "--threads") {
            a.threads = std::atoi(need_value(arg).c_str());
        } else if (arg == "--bench") {
            a.bench_dir = need_value(arg);
        } else if (arg == "--csv") {
            a.csv = need_value(arg);
        } else if (!arg.empty() && arg[0] == '-') {
            die("unknown option: " + arg);
        } else {
            if (!a.input.empty()) die("multiple input files not supported");
            a.input = arg;
        }
    }
    if (a.bench_dir.empty() && a.input.empty()) {
        print_usage(std::cerr);
        std::exit(2);
    }
    return a;
}

void print_stats(std::ostream& out, const std::string& source,
                 const qm::Stats& s) {
    out << "[qm] " << source << "\n"
        << "       on/off/dc  = " << s.input_on_terms << " / "
        << s.input_off_terms << " / " << s.input_dc_terms << "\n"
        << "       primes     = " << s.generated_primes
        << " (essentials: " << s.essential_primes << ")\n"
        << "       selected   = " << s.selected_primes << "\n"
        << "       literals   = " << s.total_literals_in << " -> "
        << s.total_literals_out << "\n"
        << "       phase A    = " << std::fixed << std::setprecision(3)
        << s.phase_a_ms << " ms\n"
        << "       phase B    = " << s.phase_b_ms << " ms\n"
        << "       total      = " << s.total_ms << " ms ("
        << s.threads_used << " threads)\n";
}

int run_single(const Args& a) {
    qm::PlaReader reader;
    qm::QuineMcCluskey minimizer;
    qm::PlaWriter writer;

    qm::Function fn;
    {
        std::ifstream in(a.input);
        if (!in) die("cannot open input: " + a.input);
        try {
            fn = reader.read(in);
        } catch (const qm::ParseError& e) {
            die(e.what());
        }
    }

    qm::MinimizerOptions opts;
    opts.threads = a.threads;
    qm::Solution sol = minimizer.minimize(fn, opts);

    if (a.output.empty()) {
        writer.write(std::cout, fn, sol);
    } else {
        std::ofstream out(a.output);
        if (!out) die("cannot open output: " + a.output);
        writer.write(out, fn, sol);
    }

    if (a.stats && !a.quiet) print_stats(std::cerr, a.input, sol.stats);
    return 0;
}

int run_bench(const Args& a) {
    if (!fs::is_directory(a.bench_dir)) {
        die("not a directory: " + a.bench_dir);
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(a.bench_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".pla") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::ofstream csv;
    if (!a.csv.empty()) {
        csv.open(a.csv);
        if (!csv) die("cannot open csv: " + a.csv);
        csv << "file,n_inputs,on_terms,primes,selected,literals_in,"
               "literals_out,phase_a_ms,phase_b_ms,total_ms,threads\n";
    }

    int failures = 0;
    for (const auto& path : files) {
        qm::PlaReader reader;
        qm::QuineMcCluskey minimizer;

        qm::Function fn;
        try {
            std::ifstream in(path);
            if (!in) {
                std::cerr << "qm: cannot open " << path << "\n";
                ++failures;
                continue;
            }
            fn = reader.read(in);
        } catch (const qm::ParseError& e) {
            std::cerr << "qm: " << path.filename().string() << ": "
                      << e.what() << "\n";
            ++failures;
            continue;
        }

        qm::MinimizerOptions opts;
        opts.threads = a.threads;
        qm::Solution sol = minimizer.minimize(fn, opts);

        if (!a.quiet) {
            print_stats(std::cerr, path.filename().string(), sol.stats);
        }

        if (csv.is_open()) {
            csv << path.filename().string() << "," << fn.n_inputs << ","
                << sol.stats.input_on_terms << ","
                << sol.stats.generated_primes << ","
                << sol.stats.selected_primes << ","
                << sol.stats.total_literals_in << ","
                << sol.stats.total_literals_out << ","
                << std::fixed << std::setprecision(3) << sol.stats.phase_a_ms
                << "," << sol.stats.phase_b_ms << "," << sol.stats.total_ms
                << "," << sol.stats.threads_used << "\n";
        }
    }
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    Args a = parse_args(argc, argv);
    return a.bench_dir.empty() ? run_single(a) : run_bench(a);
}
