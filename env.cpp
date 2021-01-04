#include <stdexcept>
#include <set>

#include "env.h"

using ref = parser::ref;
using token_type = parser::token_type;

ref env_t::load(const std::string& variable) {
    if (ctx->vars.count(variable) == 0) {
        throw std::runtime_error("undefined variable: " + variable);
    }
    return ctx->temps.pass(ctx->vars.at(variable));
}


void env_t::save(const std::string& variable, const Var& value) {
    ctx->vars[variable] = value;
}

void env_t::save(const std::string& variable, ref value) {
    save(variable, ctx->temps.pull(value));
}

ref env_t::convert(const std::string& value, token_type type) {
    switch (type) {
    case parser::tok_number:
    case parser::tok_string:
        break;
    default:
        throw std::runtime_error(std::string("unable to convert ") + parser::token_type_str[type]);
    }
    return ctx->temps.emplace(value);
}

ref env_t::scanf(ref input, const std::string& fmt, const std::vector<std::string>& varnames) {
    Var& inp = ctx->temps.pull(input);
    std::string input_string = inp->str;

    if (inp->fmt != "" || inp->varnames.size() > 0) throw std::runtime_error("multiple scanf encumbrances are not supported");

    inp->fmt = fmt;
    inp->varnames = varnames;
    return input;
}

bool scan(const std::set<char>& allowed, const char stopper, const char*& data, char* dst, bool decimal = false) {
    // skip past white space
    while (data[0] == ' ' || data[0] == '\t' || data[0] == '\n') ++data;
    // read into dst
    const char* start = dst;
    while ((data[0] != '.' || decimal) && allowed.count(data[0]) && (start == dst || data[0] == stopper)) {
        dst[0] = data[0];
        decimal = decimal && data[0] != '.';
        ++data;
        ++dst;
    }
    dst[0] = 0;
    return start < dst;
}

template<typename T> static inline void set_insert_range(std::set<T>& set, T start, T end) {
    for (T i = start; i <= end; ++i) set.insert(i);
}
template<typename T> static inline void set_insert_array(std::set<T>& set, T* arr, size_t count) {
    for (size_t i = 0; i < count; ++i) set.insert(arr[i]);
}

void var_t::read(const std::string& input_string) {
    static std::set<char>* u_set = nullptr;
    static std::set<char>* s_set = nullptr;
    if (!u_set) {
        u_set = new std::set<char>();
        s_set = new std::set<char>();
        set_insert_range(*u_set, '0', '9');
        set_insert_range(*s_set, '0', '9');
        set_insert_range(*s_set, 'a', 'z');
        set_insert_range(*s_set, 'A', 'Z');
        set_insert_array(*s_set, (char*)",./:;!@#$%^&*()-=_+[]\\{}|", 26);
    }

    size_t inpos = 0;
    size_t varnamepos = 0;
    bool fmtflag = false;
    const char* pos = input_string.data();
    char buf[128];
    std::set<char>* set;
    bool decimal = false;

    for (size_t i = 0; i < fmt.size(); ++i) {
        char ch = fmt[i];
        if (fmtflag) {
            char stopper = i + 1 < fmt.size() ? fmt[i + 1] : 0;
            switch (ch) {
            case '%':
                if (pos[0] != '%') throw std::runtime_error("format scan failure (missing % in" + input_string + ")");
                ++pos;
                break;
            case 'u': set = u_set; decimal = false; break;
            case 'f': set = u_set; decimal = true; break;
            case 's': set = s_set; decimal = false; break;
            default: throw std::runtime_error(std::string("unknown format type: %") + ch);
            }
            if (!scan(*set, stopper, pos, buf, decimal)) {
                throw std::runtime_error(std::string("failed to scan %") + ch + " from input " + input_string.data() + " near " + pos);
            }
            comps[varnames[varnamepos++]] = std::make_shared<var_t>(buf);
            if (varnamepos == varnames.size()) return;
            fmtflag = false;
        } else if (ch == '%') {
            fmtflag = true;
        } else {
            if (pos[0] != ch) throw std::runtime_error(std::string("format scan failure (missing ") + ch + " in " + input_string + ")");
            ++pos;
            break;
        }
    }

    if (varnamepos < varnames.size()) throw std::runtime_error("input ended before scanning variable " + varnames[varnamepos] + " in " + input_string + " for format " + fmt);
}

std::string var_t::write() {
    std::string rv = "";
    size_t varnamepos = 0;
    for (size_t i = 0; i < fmt.size(); ++i) {
        char ch = fmt[i];
        if (fmtflag) {
            rv += comps.at(varnames.at(varnamepos++));
            fmtflag = false;
        } else if (ch == '%') {
            fmtflag = true;
        } else rv += ch;
    }
    return rv;
}
