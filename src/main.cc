#include "qm/minimizer.hpp"
#include "qm/reader.hpp"
#include "qm/writer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
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
    std::string html;
    int threads = 0;
    bool stats = false;
    bool quiet = false;
    bool open_after = false;
};

struct BenchRecord {
    std::string file;
    std::size_t n_inputs;
    qm::Stats stats;
};

void print_usage(std::ostream& out) {
    out << "qm-minimizer — Quine-McCluskey SOP minimizer\n\n"
           "Usage:\n"
           "  qm <input.pla> [-o output.pla] [--stats] [--threads N]\n"
           "  qm --bench <dir>  [--csv file] [--html file] [--open] [--threads N]\n\n"
           "Options:\n"
           "  -o, --output FILE   Write minimized PLA to FILE (default: stdout)\n"
           "      --stats         Verbose per-run statistics on stderr\n"
           "      --threads N     Worker threads in Phase A (default: hw concurrency)\n"
           "      --bench DIR     Minimize every .pla in DIR\n"
           "      --csv FILE      With --bench: write tabular CSV summary\n"
           "      --html FILE     With --bench: write self-contained HTML dashboard\n"
           "      --open          Open the --html report in the default browser\n"
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
        } else if (arg == "--html") {
            a.html = need_value(arg);
        } else if (arg == "--open") {
            a.open_after = true;
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

void print_stats_verbose(std::ostream& out, const std::string& source,
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

void print_bench_line(std::ostream& out, const std::string& file,
                      std::size_t n, const qm::Stats& s) {
    // Compression as a ratio of input vs output literals.
    const double compression =
        s.total_literals_in > 0
            ? static_cast<double>(s.total_literals_in) / s.total_literals_out
            : 1.0;
    out << "  " << std::left << std::setw(22) << file << std::right
        << std::setw(4) << n << "v"
        << std::setw(7) << s.input_on_terms << " ON"
        << std::setw(7) << s.selected_primes << " primos"
        << "   lit "
        << std::setw(8) << s.total_literals_in << " -> "
        << std::setw(7) << s.total_literals_out
        << "  (×" << std::fixed << std::setprecision(2) << compression << ")"
        << "  " << std::setw(8) << std::setprecision(0) << s.total_ms
        << " ms\n";
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

    if (a.stats && !a.quiet) print_stats_verbose(std::cerr, a.input, sol.stats);
    return 0;
}

// ---------------------------------------------------------------------------
// HTML report
// ---------------------------------------------------------------------------

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

std::string records_to_json(const std::vector<BenchRecord>& rs) {
    std::ostringstream o;
    o << "[";
    for (std::size_t i = 0; i < rs.size(); ++i) {
        const auto& r = rs[i];
        if (i > 0) o << ",";
        o << "{\"file\":\"" << json_escape(r.file) << "\","
          << "\"n\":" << r.n_inputs << ","
          << "\"on\":" << r.stats.input_on_terms << ","
          << "\"off\":" << r.stats.input_off_terms << ","
          << "\"primes\":" << r.stats.generated_primes << ","
          << "\"essentials\":" << r.stats.essential_primes << ","
          << "\"selected\":" << r.stats.selected_primes << ","
          << "\"lit_in\":" << r.stats.total_literals_in << ","
          << "\"lit_out\":" << r.stats.total_literals_out << ","
          << "\"ms_a\":" << std::fixed << std::setprecision(3) << r.stats.phase_a_ms << ","
          << "\"ms_b\":" << r.stats.phase_b_ms << ","
          << "\"ms_total\":" << r.stats.total_ms << ","
          << "\"threads\":" << r.stats.threads_used
          << "}";
    }
    o << "]";
    return o.str();
}

std::string now_iso() {
    const auto t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

constexpr const char* HTML_TEMPLATE = R"HTML(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<title>qm-minimizer · benchmark</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
<style>
:root { --fg:#1a1a1a; --muted:#666; --line:#e1e4e8; --bg:#f7f8fa; --accent:#4c6ef5; }
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
       max-width: 1200px; margin: 2em auto; padding: 0 1.5em; color: var(--fg); }
h1 { font-size: 1.7em; margin: 0 0 0.2em; }
h2 { font-size: 0.85em; margin: 2em 0 0.6em; color: var(--muted);
     text-transform: uppercase; letter-spacing: 0.08em; }
.sub { color: var(--muted); margin: 0 0 1.5em; font-size: 0.95em; }
.summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 0.8em; }
.card { background: var(--bg); padding: 1em 1.2em; border-radius: 8px; }
.card .label { font-size: 0.75em; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; }
.card .value { font-size: 1.5em; font-weight: 600; margin-top: 0.2em; font-variant-numeric: tabular-nums; }
.card .hint { font-size: 0.8em; color: var(--muted); margin-top: 0.2em; }
.row { display: grid; grid-template-columns: 1fr 1fr; gap: 1.2em; margin: 0.8em 0; }
@media (max-width: 800px) { .row { grid-template-columns: 1fr; } }
.chart-card { background: white; border: 1px solid var(--line); border-radius: 8px; padding: 0.8em 1em 1em; }
.chart-card .title { font-size: 0.85em; color: var(--fg); margin: 0 0 0.5em; font-weight: 600; }
.chart-card .note { font-size: 0.75em; color: var(--muted); margin: 0.4em 0 0; }
table { width: 100%; border-collapse: collapse; font-size: 0.88em; }
th { text-align: left; padding: 0.6em 0.5em; background: var(--bg); border-bottom: 2px solid var(--line);
     font-weight: 600; font-size: 0.72em; text-transform: uppercase; color: var(--muted); letter-spacing: 0.05em; }
td { padding: 0.55em 0.5em; border-bottom: 1px solid #f0f0f0; font-variant-numeric: tabular-nums; }
td.num, th.num { text-align: right; }
.footer { color: #aaa; margin-top: 3em; font-size: 0.8em; text-align: center; }
.banner { background: #fff8e1; border-left: 3px solid #f4b400; padding: 0.8em 1em;
          border-radius: 4px; margin: 1em 0; font-size: 0.9em; color: #6b5b1d; }
.banner strong { color: #4a3d12; }
</style>
</head>
<body>
  <h1>qm-minimizer · benchmark</h1>
  <p class="sub">__N_CASES__ casos · __N_THREADS__ threads · __TOTAL_S__ s totais · __TIMESTAMP__</p>

  <div id="banner" class="banner" style="display:none"></div>

  <h2>Resumo</h2>
  <div class="summary">
    <div class="card"><div class="label">Casos</div><div class="value" id="s-cases">–</div></div>
    <div class="card"><div class="label">Tempo total</div><div class="value" id="s-total">–</div></div>
    <div class="card"><div class="label">Mais lento</div><div class="value" id="s-slow">–</div><div class="hint" id="s-slow-h"></div></div>
    <div class="card"><div class="label">Mais rápido</div><div class="value" id="s-fast">–</div><div class="hint" id="s-fast-h"></div></div>
    <div class="card"><div class="label">Compressão média</div><div class="value" id="s-comp">–</div><div class="hint">literais in / out</div></div>
  </div>

  <div class="row">
    <div class="chart-card">
      <p class="title">Tempo total por caso (ms)</p>
      <canvas id="chart-total"></canvas>
    </div>
    <div class="chart-card">
      <p class="title">Fase A vs Fase B (ms, empilhado)</p>
      <canvas id="chart-phases"></canvas>
      <p class="note">A = geração de primos · B = cobertura</p>
    </div>
  </div>

  <div class="row">
    <div class="chart-card">
      <p class="title">Escalabilidade: tempo vs variáveis de entrada</p>
      <canvas id="chart-scale"></canvas>
      <p class="note">Eixos lineares; espera-se crescimento sub-linear em n e linear/quadrático em |ON|</p>
    </div>
    <div class="chart-card">
      <p class="title">Compressão de literais</p>
      <canvas id="chart-compression"></canvas>
      <p class="note">Se entrada ≈ saída em todos os casos: nenhum primo cresceu (caso esparso, ADR 0005)</p>
    </div>
  </div>

  <h2>Detalhes</h2>
  <table id="data-table">
    <thead>
      <tr>
        <th>arquivo</th>
        <th class="num">n</th>
        <th class="num">ON</th>
        <th class="num">OFF</th>
        <th class="num">primos</th>
        <th class="num">essenciais</th>
        <th class="num">selec.</th>
        <th class="num">lit. in</th>
        <th class="num">lit. out</th>
        <th class="num">A (ms)</th>
        <th class="num">B (ms)</th>
        <th class="num">total (ms)</th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>

  <p class="footer">qm-minimizer · gerado em __TIMESTAMP__</p>

<script>
const data = __DATA_JSON__;
const labels = data.map(d => d.file);
const totals = data.map(d => d.ms_total);
const fmt = (n, p=0) => n.toLocaleString('pt-BR', { maximumFractionDigits: p });

// Summary cards
const total = totals.reduce((a, b) => a + b, 0);
const slow = data.reduce((m, d) => d.ms_total > m.ms_total ? d : m, data[0]);
const fast = data.reduce((m, d) => d.ms_total < m.ms_total ? d : m, data[0]);
const litIn = data.reduce((a, d) => a + d.lit_in, 0);
const litOut = data.reduce((a, d) => a + d.lit_out, 0);
const compression = litOut > 0 ? litIn / litOut : 0;
document.getElementById('s-cases').textContent = data.length;
document.getElementById('s-total').textContent = fmt(total / 1000, 2) + ' s';
document.getElementById('s-slow').textContent = fmt(slow.ms_total) + ' ms';
document.getElementById('s-slow-h').textContent = slow.file;
document.getElementById('s-fast').textContent = fmt(fast.ms_total) + ' ms';
document.getElementById('s-fast-h').textContent = fast.file;
document.getElementById('s-comp').textContent = compression.toFixed(2) + '×';

// Sanity banner — flag when no compression happens at all
if (compression < 1.01) {
  const banner = document.getElementById('banner');
  banner.style.display = 'block';
  banner.innerHTML = '<strong>Observação:</strong> compressão de literais ≈ 1× em todos os casos. ' +
                     'Sintoma esperado em funções esparsas: QM clássico não funde minterms que ' +
                     'não diferem em 1 bit, e nesses benchmarks (6400 termos em 2<sup>n</sup> ' +
                     'inputs com n ≥ 32) isso é a regra. Ver <code>docs/adr/0005-implicit-dont-cares.md</code>.';
}

// Table
const tbody = document.querySelector('#data-table tbody');
data.forEach(d => {
  const tr = document.createElement('tr');
  tr.innerHTML = `
    <td>${d.file}</td>
    <td class="num">${d.n}</td>
    <td class="num">${d.on}</td>
    <td class="num">${d.off}</td>
    <td class="num">${d.primes}</td>
    <td class="num">${d.essentials}</td>
    <td class="num">${d.selected}</td>
    <td class="num">${fmt(d.lit_in)}</td>
    <td class="num">${fmt(d.lit_out)}</td>
    <td class="num">${fmt(d.ms_a, 1)}</td>
    <td class="num">${fmt(d.ms_b, 1)}</td>
    <td class="num">${fmt(d.ms_total, 1)}</td>
  `;
  tbody.appendChild(tr);
});

const palette = { blue:'rgba(76,110,245,0.78)', teal:'rgba(20,184,166,0.78)',
                  amber:'rgba(245,158,11,0.78)', rose:'rgba(244,63,94,0.62)' };
const baseOpts = { responsive: true, maintainAspectRatio: true,
                   plugins: { legend: { labels: { font: { size: 11 } } } } };

new Chart(document.getElementById('chart-total'), {
  type: 'bar',
  data: { labels, datasets: [{ label:'Total (ms)', data: totals, backgroundColor: palette.blue }] },
  options: { ...baseOpts, plugins: { legend: { display: false } } }
});

new Chart(document.getElementById('chart-phases'), {
  type: 'bar',
  data: {
    labels,
    datasets: [
      { label: 'Fase A', data: data.map(d => d.ms_a), backgroundColor: palette.teal },
      { label: 'Fase B', data: data.map(d => d.ms_b), backgroundColor: palette.amber }
    ]
  },
  options: { ...baseOpts, scales: { x: { stacked: true }, y: { stacked: true } } }
});

new Chart(document.getElementById('chart-scale'), {
  type: 'scatter',
  data: { datasets: [{
    label: 'Casos', data: data.map(d => ({ x: d.n, y: d.ms_total, file: d.file })),
    backgroundColor: palette.blue, pointRadius: 6
  }]},
  options: { ...baseOpts,
    scales: { x: { title: { display: true, text: 'n variáveis' } },
              y: { title: { display: true, text: 'tempo total (ms)' } } },
    plugins: { legend: { display: false },
               tooltip: { callbacks: { label: (ctx) =>
                 `${ctx.raw.file}  (n=${ctx.parsed.x}, ${ctx.parsed.y.toFixed(0)} ms)` } } }
  }
});

new Chart(document.getElementById('chart-compression'), {
  type: 'bar',
  data: {
    labels,
    datasets: [
      { label: 'Literais entrada', data: data.map(d => d.lit_in), backgroundColor: palette.rose },
      { label: 'Literais saída', data: data.map(d => d.lit_out), backgroundColor: palette.blue }
    ]
  },
  options: baseOpts
});
</script>
</body>
</html>
)HTML";

void replace_all(std::string& s, const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

void write_html_report(const std::string& path,
                       const std::vector<BenchRecord>& records,
                       int threads_used) {
    std::string out = HTML_TEMPLATE;
    const std::string json = records_to_json(records);
    double total_ms = 0;
    for (const auto& r : records) total_ms += r.stats.total_ms;
    std::ostringstream total_s;
    total_s << std::fixed << std::setprecision(2) << (total_ms / 1000.0);

    replace_all(out, "__DATA_JSON__", json);
    replace_all(out, "__N_CASES__", std::to_string(records.size()));
    replace_all(out, "__N_THREADS__", std::to_string(threads_used));
    replace_all(out, "__TOTAL_S__", total_s.str());
    replace_all(out, "__TIMESTAMP__", now_iso());

    std::ofstream f(path);
    if (!f) die("cannot open html: " + path);
    f << out;
}

bool open_in_browser(const std::string& path) {
#if defined(__APPLE__)
    const std::string cmd = "open \"" + path + "\"";
#elif defined(_WIN32)
    const std::string cmd = "start \"\" \"" + path + "\"";
#else
    const std::string cmd = "xdg-open \"" + path + "\"";
#endif
    return std::system(cmd.c_str()) == 0;
}

// ---------------------------------------------------------------------------
// Bench mode
// ---------------------------------------------------------------------------

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

    if (!a.quiet) {
        std::cerr << "qm: " << files.size() << " arquivo"
                  << (files.size() == 1 ? "" : "s") << " em "
                  << a.bench_dir << "\n";
    }

    std::ofstream csv;
    if (!a.csv.empty()) {
        csv.open(a.csv);
        if (!csv) die("cannot open csv: " + a.csv);
        csv << "file,n_inputs,on_terms,off_terms,primes,essentials,selected,"
               "literals_in,literals_out,phase_a_ms,phase_b_ms,total_ms,"
               "threads\n";
    }

    std::vector<BenchRecord> records;
    records.reserve(files.size());
    int failures = 0;
    int threads_used = 1;

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
        threads_used = sol.stats.threads_used;

        if (!a.quiet) {
            if (a.stats) print_stats_verbose(std::cerr,
                                             path.filename().string(),
                                             sol.stats);
            else print_bench_line(std::cerr, path.filename().string(),
                                  fn.n_inputs, sol.stats);
        }

        if (csv.is_open()) {
            csv << path.filename().string() << "," << fn.n_inputs << ","
                << sol.stats.input_on_terms << ","
                << sol.stats.input_off_terms << ","
                << sol.stats.generated_primes << ","
                << sol.stats.essential_primes << ","
                << sol.stats.selected_primes << ","
                << sol.stats.total_literals_in << ","
                << sol.stats.total_literals_out << ","
                << std::fixed << std::setprecision(3) << sol.stats.phase_a_ms
                << "," << sol.stats.phase_b_ms << "," << sol.stats.total_ms
                << "," << sol.stats.threads_used << "\n";
        }

        records.push_back({path.filename().string(), fn.n_inputs, sol.stats});
    }

    if (!a.quiet) {
        double total_ms = 0;
        for (const auto& r : records) total_ms += r.stats.total_ms;
        std::cerr << "qm: " << records.size() << " ok, " << failures
                  << " falha" << (failures == 1 ? "" : "s") << ", "
                  << std::fixed << std::setprecision(2) << (total_ms / 1000.0)
                  << " s totais (" << threads_used << " threads)\n";
    }

    if (!a.html.empty()) {
        write_html_report(a.html, records, threads_used);
        if (!a.quiet) std::cerr << "qm: relatório em " << a.html << "\n";
        if (a.open_after) open_in_browser(a.html);
    }

    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    Args a = parse_args(argc, argv);
    return a.bench_dir.empty() ? run_single(a) : run_bench(a);
}
