#include <stdexcept>
#include <set>

#include "env.h"
#include "utils.h"

using parser::token_type;

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
    ctx->varlist.push_back(value);
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

ref env_t::constant(const std::string& value, token_type type, const std::map<std::string,std::string>& exceptions) {
    switch (type) {
    case parser::tok_number:
    case parser::tok_string:
        break;
    default:
        throw std::runtime_error(std::string("unable to convert ") + parser::token_type_str[type]);
    }
    return ctx->temps.emplace(value, type == parser::tok_number, exceptions);
}

ref env_t::scanf(const std::string& input, const std::string& fmt, const std::vector<prioritized_t>& varnames) {
    Var tmp = std::make_shared<var_t>(input);
    tmp->fmt = fmt;
    tmp->varnames = varnames;
    return ctx->temps.pass(tmp);
}

ref env_t::sum(ref value) {
    Var tmp = std::make_shared<var_t>(*ctx->temps.pull(value));
    if (tmp->aggregates) throw std::runtime_error("invalid operation (aggregating variable cannot be aggregated further)");
    tmp->aggregates = true;
    return ctx->temps.pass(tmp);
}

void env_t::declare_aspects(const std::vector<prioritized_t>& aspects, const std::string& source) {
    if (ctx->aspects.size() > 0) throw std::runtime_error("multiple aspects statements are invalid");
    ctx->aspects = aspects;
    ctx->aspect_source = source;
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
    if (inp->helper) throw std::runtime_error("helpers cannot also be keys");
    inp->key = true;
    return source;
}

ref env_t::helper(ref source) {
    Var& inp = ctx->temps.pull(source);
    if (inp->helper) throw std::runtime_error("duplicate helper assignment to variable");
    if (inp->key) throw std::runtime_error("keys cannot also be helpers");
    inp->helper = true;
    return source;
}

// ref env_t::exceptions(const std::map<std::string,std::string>& list) {
//     Var exceptvar = std::make_shared<var_t>();
//     exceptvar->exceptions = list;
//     return ctx->temps.pass(exceptvar);
// }

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

    if (exceptions.count(value)) value = exceptions[value];

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
            comps[varnames[varnamepos++].label] = buf;
            if (varnamepos == varnames.size()) return;
            fmtflag = false;
        } else if (ch == '%') {
            fmtflag = true;
        } else {
            if (pos[0] != ch) throw std::runtime_error(std::string("format scan failure (missing ") + ch + " in " + input_string + ")");
            ++pos;
        }
    }

    if (varnamepos < varnames.size()) throw std::runtime_error("input ended before scanning variable " + varnames[varnamepos].label + " in " + input_string + " for format " + fmt);
}

Value var_t::imprint(std::set<std::string>& fitness_set) const {
    mutable_val_t val;
    val.value = value;
    val.numeric = is_numeric(value.c_str());
    if (fmt.size() > 0) {
        val.comps.clear();
        for (size_t i = 0; i < varnames.size(); ++i) {
            val.comps[varnames[i].label] = prioritized_t(comps.at(varnames[i].label), varnames.at(i).priority);
        }
    }
    if (fit.size() > 0) {
        std::vector<std::string> versions;
        for (const auto& f : fit) versions.push_back(f->write());
        size_t i = 0, first = 0;
        for (; i < versions.size(); ++i) {
            if (versions[i] == "") {
                first += first == i;
                continue;
            }
            if (fitness_set.count(versions[i])) {
                break;
            }
        }
        if (i == versions.size()) {
            // insert
            fitness_set.insert(versions[first]);
            i = first;
        }
        val.value = versions[i];
        versions.erase(versions.begin() + i);
        val.alternatives = versions;
    }
    return std::make_shared<val_t>(val);
}

void var_t::read(const val_t& val) {
    const auto& val_comps = val.get_comps();
    if (val_comps.size() > 0) {
        comps.clear();
        for (size_t i = 0; i < varnames.size(); ++i) {
            comps[varnames[i].label] = val_comps.at(varnames[i].label).label;
        }
        value = write();
    } else if (fit.size() > 0) {
        std::vector<std::string> versions;
        versions.push_back(val.get_value());
        versions.insert(versions.end(), val.alternatives.begin(), val.alternatives.end());
        if (versions.size() != fit.size()) throw std::runtime_error("fit error (" + std::to_string(versions.size()) + " != " + std::to_string(fit.size()) + ")");
        // alternatives.clear();
        for (size_t i = 0; i < fit.size(); ++i) {
            fit[i]->read(versions[i]);
            // alternatives.push_back(fit[i]->imprint());
        }
    } else {
        value = val.get_value();
        if (exceptions.count(value)) {
            value = exceptions[value];
        }
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
                    rv += comps.at(varnames.at(varnamepos++).label);
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
    if (exceptions.size() > 0) {
        suffix += " except { ";
        for (const auto& m : exceptions) {
            suffix += m.first + " == " + m.second + ", ";
        }
        suffix += "}";
    }
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

bool var_t::operator<(const var_t& other) const {
    return other.write() < write();
}

std::string val_t::to_string() const {
    std::string s = "";
    if (comps.size() > 0) {
        for (const auto& c : comps) {
            s += (s == "" ? "" : ", ") + c.first + "=" + c.second.to_string();
        }
        return "{ " + s + " }";
    }
    if (alternatives.size() > 0) {
        s = get_value();
        for (const auto& a : alternatives) {
            s += "|" + a;
        }
        return s;
    }
    return get_value();
}

void val_t::did_change() {
    size_t bytes = comps.size() > 0 ? 0 : numeric ? sizeof(number) : _value.size();
    for (const auto& c : comps) bytes += c.second.label.size();
    if (complen == 0) {
        comparable = (uint8_t*)malloc(bytes);
    } else if (complen < bytes) {
        comparable = (uint8_t*)realloc(comparable, bytes);
    }
    complen = bytes;
    if (numeric) {
        memcpy(comparable, &number, sizeof(number));
        return;
    }
    if (comps.size() == 0) {
        memcpy(comparable, _value.c_str(), _value.size());
        return;
    }
    uint8_t* pos = comparable;
    std::vector<std::string> order;
    order.resize(comps.size());
    for (const auto& i : comps) {
        order[i.second.priority] = i.first;
    }
    for (const auto& c : order) {
        const auto& a = comps.at(c).label;
        memcpy(pos, a.c_str(), a.size());
        pos += a.size();
    }
}

const std::string& val_t::get_value() const {
    if (numeric && number != cached_number) {
        cached_number = number;
        _value = std::to_string(number);
    }
    return _value;
}
const std::map<std::string, prioritized_t>& val_t::get_comps() const { return comps; }
void val_t::set_value(const std::string& new_value) { _value = new_value; did_change(); }
void val_t::set_comps(const std::map<std::string, prioritized_t>& new_comps, const std::string& new_value) { comps = new_comps; _value = new_value; did_change(); }
bool val_t::is_number() const { return numeric; }
int64_t val_t::get_number() const { return number; }
void val_t::set_number(int64_t v) {
    number = v;
    did_change();
}

bool val_t::operator<(const val_t& other) const {
    int c = memcmp(comparable, other.comparable, complen > other.complen ? other.complen : complen);
    return c ? c < 0 : other.complen > complen;
}

bool val_t::fits(const val_t& value) const {
    if (alternatives.size() == 0) {
        if (value.alternatives.size() > 0) {
            return value.fits(*this);
        }
        return !(*this < value) && !(value < *this);
    }
    if (comps.size() > 0) {
        throw std::runtime_error("components and fitted vars are unsupported");
    }
    for (const auto& a : alternatives) {
        if (value._value == a) return true;
    }
    return false;
}

Value val_t::clone() const {
    Value val = std::make_shared<val_t>(*this);
    val->comparable = (uint8_t*)malloc(complen);
    memcpy(val->comparable, comparable, complen);
    return val;
}

void val_t::aggregate(const val_t& v, uint8_t curr_phase) {
    if (!v.numeric) {
        v.number = v.int64();
        v.numeric = true;
    }
    if (curr_phase != phase) {
        numeric = true;
        number = v.number;
        phase = curr_phase;
        return;
    }
    if (!numeric) {
        number = int64();
        numeric = true;
    }
    number += v.number;
}
