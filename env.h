#include <map>
#include <memory>

#include "parser/parser.h"

using parser::prioritized_t;
using parser::ref;

struct mutable_val_t {
    std::string value;
    std::map<std::string, prioritized_t> comps;
    bool numeric{false};
};

class val_t {
private:
    mutable std::string _value;
    std::map<std::string, prioritized_t> comps;
    uint8_t* comparable;
    size_t complen;
    mutable int64_t number;
    mutable int64_t cached_number{-1};
    mutable bool numeric;
    void did_change();
    // this differs from get_number() in that it forcibly converts value, whereas get_number() assumes numeric=true
    int64_t int64() const { int64_t x = cached_number; cached_number = number; auto rv = (int64_t)atoll(get_value().c_str()); cached_number = x; return rv; }
public:
    uint8_t phase{0};
    val_t(const mutable_val_t& mv) : _value(mv.value), comps(mv.comps), comparable(nullptr), complen(0), numeric(mv.numeric) {
        if (numeric) number = int64(); did_change();
    }
    val_t(const std::string& value_in = "") : _value(value_in), comparable(nullptr), complen(0) { did_change(); }
    std::shared_ptr<val_t> clone() const;
    const std::string& get_value() const;
    const std::map<std::string, prioritized_t>& get_comps() const;
    void set_value(const std::string& new_value);
    void set_comps(const std::map<std::string, prioritized_t>& comps, const std::string& new_value = "");
    bool operator<(const val_t& other) const;
    std::string to_string() const;
    void aggregate(const val_t& v, uint8_t curr_phase);
    bool is_number() const;
    int64_t get_number() const;
    void set_number(int64_t v);
};

typedef std::shared_ptr<val_t> Value;

struct var_t {
    std::string str; // this is the string associated with this variable, e.g. "date" or "Province/Region".
    std::string value; // this is the actual value at the moment (e.g. "2021-01-05")
    std::vector<std::shared_ptr<var_t>> fit; // this is a fit vector for combining multiple fields
    std::map<std::string, std::string> comps;
    int index{-1};
    bool trails{false};
    bool key{false};
    bool numeric{false};
    bool aggregates{false};
    bool helper{false};

    ref pref{0};
    std::string fmt;
    std::vector<prioritized_t> varnames;
    var_t(const std::string& str_in = "") : str(str_in) {}
    var_t(const std::string& str_in, bool numeric_in) : str(str_in), numeric(numeric_in) {}
    void read(const std::string& input);
    std::string write() const;
    std::string to_string() const;
    bool operator<(const var_t& other) const;
    Value imprint() const;
    void read(const val_t& val);
};

typedef std::shared_ptr<var_t> Var;

struct tempstore_t {
    std::vector<Var> store;
    tempstore_t() {
        store.push_back(Var(nullptr));
    }
    ref pass(Var& v) {
        if (v->pref) return v->pref;
        store.push_back(v);
        return store.size() - 1;
    }
    ref retain(const Var& v) {
        store.push_back(v);
        return store.size() - 1;
    }
    ref emplace(const std::string& value, bool numeric = false) {
        return retain(std::make_shared<var_t>(value, numeric));
    }
    inline Var& pull(ref r) {
        for (;;) {
            Var v = store[r];
            if (v->pref && v->pref != r) { r = v->pref; continue; }
            return store[r];
        }
    }
};

struct context_t {
    std::vector<Var> varlist;
    std::map<std::string,Var> vars;
    std::map<Var,std::string> varnames;
    std::map<std::string,std::string> links;
    std::vector<prioritized_t> aspects;
    std::string aspect_source;
    Var trailing;
    tempstore_t temps;
};

typedef std::shared_ptr<context_t> Context;

struct env_t: public parser::st_callback_table {
    Context ctx;

    env_t() {
        ctx = std::make_shared<context_t>();
    }

    ref load(const std::string& variable) override;
    void save(const std::string& variable, const Var& value);
    void save(const std::string& variable, ref value) override;
    ref constant(const std::string& value, parser::token_type type) override;
    ref scanf(const std::string& input, const std::string& fmt, const std::vector<prioritized_t>& varnames) override;
    ref sum(ref value) override;
    // void declare(const std::string& key, const std::string& value) override;
    void declare_aspects(const std::vector<prioritized_t>& aspects, const std::string& source) override;
    ref fit(const std::vector<std::string>& sources) override;
    ref key(ref source) override;
    ref helper(ref source) override;
};
