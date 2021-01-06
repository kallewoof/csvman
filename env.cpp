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
    printf("  [] %s = %s\n", variable.c_str(), value->to_string().c_str());
    ctx->vars[variable] = value;
    ctx->varnames[value] = variable;
    if (value->str == "*") value->trails = true;
    if (value->fit.size() == 0 && !value->numeric) {
        if (value->str == "*") {
            if (ctx->trailing.get()) {
                throw std::runtime_error("multiple trailing declarations not supported");
            }
            value->trails = true;
            ctx->trailing = value;
        } else {
            ctx->links[value->str] = variable;
        }
    }
}

void env_t::save(const std::string& variable, ref value) {
    save(variable, ctx->temps.pull(value));
}

ref env_t::constant(const std::string& value, token_type type) {
    switch (type) {
    case parser::tok_number:
    case parser::tok_string:
        break;
    default:
        throw std::runtime_error(std::string("unable to convert ") + parser::token_type_str[type]);
    }
    return ctx->temps.emplace(value, type == parser::tok_number);
}

ref env_t::scanf(const std::string& input, const std::string& fmt, const std::vector<std::string>& varnames) {
    Var tmp = std::make_shared<var_t>(input);
    tmp->fmt = fmt;
    tmp->varnames = varnames;
    return ctx->temps.pass(tmp);
}

// void env_t::declare(const std::string& key, const std::string& value) {
//     if (key == "layout") {
//         if (value == "vertical") ctx->layout = layout_t::vertical;
//         else if (value == "horizontal") ctx->layout = layout_t::horizontal;
//         else throw std::runtime_error("unknown layout: " + value);
//         return;
//     }
//     throw std::runtime_error("unknown declaration key " + key);
// }

void env_t::declare_aspects(const std::vector<std::string>& aspects) {
    if (ctx->aspects.size() > 0) throw std::runtime_error("multiple aspects statements are invalid");
    ctx->aspects = aspects;
}

ref env_t::fit(const std::vector<std::string>& sources) {
    Var fitted = std::make_shared<var_t>();
    for (auto c : sources) {
        if (ctx->vars.count(c) == 0) throw std::runtime_error("unknown variable " + c + " in fit operation");
        auto v = ctx->vars[c];
        fitted->fit.push_back(v);
    }
    return ctx->temps.pass(fitted);
}

ref env_t::key(ref source) {
    Var& inp = ctx->temps.pull(source);
    if (inp->key) throw std::runtime_error("duplicate key assignment to variable");
    inp->key = true;
    return source;
}

bool scan(const std::set<char>& allowed, const char stopper, const char*& data, char* dst, bool decimal = false) {
    // skip past white space
    while (data[0] == ' ' || data[0] == '\t' || data[0] == '\n') ++data;
    // read into dst
    const char* start = dst;
    while ((data[0] != '.' || decimal) && allowed.count(data[0]) && (start == dst || data[0] != stopper)) {
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
    value = input_string;

    if (fit.size() > 0) {
        const char* ch = input_string.data();
        size_t s = 0, p = 0;
        for (size_t i = 0; i < fit.size(); ++i) {
            while (input_string[p] && input_string[p] != '|') ++p;
            fit[i]->read(std::string(&ch[s], &ch[p]));
            ++p;
        }
        return;
    }

    if (fmt == "") return;

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
            comps[varnames[varnamepos++]] = buf;
            if (varnamepos == varnames.size()) return;
            fmtflag = false;
        } else if (ch == '%') {
            fmtflag = true;
        } else {
            if (pos[0] != ch) throw std::runtime_error(std::string("format scan failure (missing ") + ch + " in " + input_string + ")");
            ++pos;
        }
    }

    if (varnamepos < varnames.size()) throw std::runtime_error("input ended before scanning variable " + varnames[varnamepos] + " in " + input_string + " for format " + fmt);
}

Value var_t::imprint() const {
    Value val = std::make_shared<val_t>();
    val->value = value;
    if (fmt.size() > 0) {
        val->comps = comps;
        return val;
    }
    if (fit.size() > 0) {
        std::vector<std::string> versions;
        for (const auto& f : fit) versions.push_back(f->write());
        val->value = versions.back();
        versions.pop_back();
        val->alternatives = versions;
    }
    return val;
}

void var_t::read(const val_t& val) {
    if (val.comps.size() == 0) {
        value = val.value;
    } else {
        comps = val.comps;
        value = write();
    }
}

std::string var_t::write() const {
    if (fmt.size() > 0) {
        std::string rv = "";
        size_t varnamepos = 0;
        bool fmtflag = false;
        for (size_t i = 0; i < fmt.size(); ++i) {
            char ch = fmt[i];
            if (fmtflag) {
                if (ch == '%') {
                    rv += '%';
                } else {
                    rv += comps.at(varnames.at(varnamepos++));
                }
                fmtflag = false;
            } else if (ch == '%') {
                fmtflag = true;
            } else rv += ch;
        }
        return rv;
    }
    if (fit.size() > 0) {
        std::string rv = "{";
        for (size_t i = 0; i < fit.size(); ++i) {
            rv += (i ? "|" : "") + fit[i]->write();
        }
        rv += "}";
        return rv;
    }
    return value;
}

std::string var_t::to_string() const {
    std::string suffix = "";
    if (index != -1) suffix += " (@" + std::to_string(index) + ")";
    if (key) suffix += " (key)";
    if (numeric) suffix += " (num)";
    if (trails) suffix += " (trails)";
    if (fit.size() > 0) {
        std::string s = "fit {";
        for (const auto& f : fit) s += "\n\t" + f->to_string();
        return s + "}" + suffix;
    }
    if (fmt == "") return str + suffix;
    if (comps.size() == 0) {
        return str + " (" + std::to_string(varnames.size()) + " component scanned)" + suffix;
    }
    return str + "(" + write() + ")" + suffix;
}

std::string val_t::to_string() const {
    std::string s = "";
    if (comps.size() > 0) {
        for (const auto& c : comps) {
            s += (s == "" ? "" : ", ") + c.first + "=" + c.second;
        }
        return "{ " + s + " }";
    }
    if (alternatives.size() > 0) {
        s = value;
        for (const auto& a : alternatives) {
            s += "|" + a;
        }
        return s;
    }
    return value;
}

bool var_t::operator<(const var_t& other) const {
    if (fit.size() > 0) {
        if (other.fit[0]->write()<fit[0]->write()) return false;
        for (size_t i = 0; i < fit.size(); ++i) {
            auto a = fit[i]->write();
            for (size_t j = 0; j < other.fit.size(); ++j) {
                auto b = other.fit[j]->write();
                if (a == b) return false;
            }
        }
        return true;
    }
    return other.write() < write();
}

bool val_t::operator<(const val_t& other) const {
    std::vector<std::string> a, b;
    if (alternatives.size() == 0) {
        if (comps.size() > 0) {
            for (const auto& i : comps) {
                if (i.second < other.comps.at(i.first)) return true;
                if (other.comps.at(i.first) < i.second) return false;
            }
            return false;
        }
        return (value < other.value);
    }
    a = alternatives;
    a.push_back(value);
    b = other.alternatives;
    b.push_back(other.value);
    int rv = -1;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] < b[i]) {
            if (rv == -1) rv = 1;
        } else if (b[i] < a[i]) {
            if (rv == -1) rv = 0;
        } else {
            // they are equal
            return false;
        }
    }
    return rv == 1;
}
