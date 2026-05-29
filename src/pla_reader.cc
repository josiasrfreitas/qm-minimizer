#include "qm/reader.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace qm {

ParseError::ParseError(std::size_t line, std::string message)
    : std::runtime_error("PLA parse error at line " + std::to_string(line) +
                         ": " + message),
      line_(line) {}

namespace {

std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

// Strip a leading '#' comment (and trailing whitespace).
std::string strip_comment(std::string s) {
    auto pos = s.find('#');
    if (pos != std::string::npos) s.erase(pos);
    return trim(std::move(s));
}

std::vector<std::string> split_ws(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(std::move(tok));
    return out;
}

PlaType parse_type(const std::string& v) {
    if (v == "f" || v == "F") return PlaType::F;
    if (v == "fr" || v == "FR") return PlaType::FR;
    if (v == "fd" || v == "FD") return PlaType::FD;
    return PlaType::F;  // lenient fallback
}

void apply_input_pattern(const std::string& pat, Term& t,
                         std::size_t line_no) {
    const std::size_t n = t.n_inputs();
    if (pat.size() != n) {
        throw ParseError(line_no,
                         "input pattern length " + std::to_string(pat.size()) +
                             " does not match .i = " + std::to_string(n));
    }
    for (std::size_t i = 0; i < n; ++i) {
        switch (pat[i]) {
            case '0':
                t.mask.set(i, true);
                t.value.set(i, false);
                break;
            case '1':
                t.mask.set(i, true);
                t.value.set(i, true);
                break;
            case '-':
            case '~':
            case '2':  // some dialects use '2' for don't-care
                t.mask.set(i, false);
                t.value.set(i, false);
                break;
            default:
                throw ParseError(line_no,
                                 std::string("invalid input character '") +
                                     pat[i] + "'");
        }
    }
}

// Routes a parsed term to on/off/dc based on the function type and the
// listed output character.
void route_term(Function& f, Term&& t, char out_char, std::size_t line_no) {
    switch (out_char) {
        case '1':
            f.on_set.push_back(std::move(t));
            return;
        case '0':
            f.off_set.push_back(std::move(t));
            return;
        case '-':
        case '~':
        case '2':
            f.dc_set.push_back(std::move(t));
            return;
        default:
            throw ParseError(
                line_no,
                std::string("invalid output character '") + out_char + "'");
    }
}

}  // namespace

Function PlaReader::read(std::istream& in) {
    Function f;
    std::string raw;
    std::size_t line_no = 0;
    bool inputs_set = false;
    bool outputs_set = false;
    bool type_set = false;

    while (std::getline(in, raw)) {
        ++line_no;
        std::string line = strip_comment(std::move(raw));
        if (line.empty()) continue;

        if (line[0] == '.') {
            auto toks = split_ws(line);
            const std::string& dir = toks[0];
            if (dir == ".i") {
                if (toks.size() < 2)
                    throw ParseError(line_no, ".i requires an argument");
                f.n_inputs = static_cast<std::size_t>(std::stoul(toks[1]));
                inputs_set = true;
            } else if (dir == ".o") {
                if (toks.size() < 2)
                    throw ParseError(line_no, ".o requires an argument");
                f.n_outputs = static_cast<std::size_t>(std::stoul(toks[1]));
                if (f.n_outputs != 1) {
                    throw ParseError(
                        line_no,
                        "multi-output PLAs are out of scope (got .o " +
                            std::to_string(f.n_outputs) + ")");
                }
                outputs_set = true;
            } else if (dir == ".p") {
                // hint, ignored for parsing correctness
            } else if (dir == ".type") {
                if (toks.size() < 2)
                    throw ParseError(line_no, ".type requires an argument");
                f.type = parse_type(toks[1]);
                type_set = true;
            } else if (dir == ".ilb") {
                f.input_names.assign(toks.begin() + 1, toks.end());
            } else if (dir == ".ob") {
                f.output_names.assign(toks.begin() + 1, toks.end());
            } else if (dir == ".e" || dir == ".end") {
                break;
            } else if (dir == ".kiss" || dir == ".r" || dir == ".s" ||
                       dir == ".phase") {
                throw ParseError(
                    line_no, "unsupported directive: " + dir);
            } else {
                // silently skip other unknown directives to stay friendly
            }
            continue;
        }

        if (!inputs_set || !outputs_set) {
            throw ParseError(line_no,
                             "product line before .i / .o declaration");
        }

        auto toks = split_ws(line);
        if (toks.size() < 2) {
            throw ParseError(line_no,
                             "expected '<input> <output>' on product line");
        }
        const std::string& in_pat = toks[0];
        const std::string& out_pat = toks[1];
        if (out_pat.size() != 1) {
            throw ParseError(line_no,
                             "output pattern must be a single character for "
                             ".o 1");
        }

        Term t(BitVec(f.n_inputs), BitVec(f.n_inputs));
        apply_input_pattern(in_pat, t, line_no);
        route_term(f, std::move(t), out_pat[0], line_no);
    }

    if (!inputs_set) throw ParseError(line_no, "missing .i directive");
    if (!outputs_set) throw ParseError(line_no, "missing .o directive");

    // Default type: when not specified, follow Espresso convention.
    (void)type_set;
    return f;
}

}  // namespace qm
