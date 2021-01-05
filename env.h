#include <map>
#include <memory>

#include "parser/parser.h"

struct var_t {
    std::string str;
    std::vector<std::string> fit;
    std::map<std::string, std::string> comps;
    size_t index{0};
    bool trails{false};
    bool key{false};
    bool numeric{false};

    parser::ref pref{0};
    std::string fmt;
    std::vector<std::string> varnames;
    var_t(const std::string& str_in = "") : str(str_in) {}
    var_t(const std::string& str_in, bool numeric_in) : str(str_in), numeric(numeric_in) {}
    void read(const std::string& input);
    std::string write() const;
    std::string to_string() const;
};

typedef std::shared_ptr<var_t> Var;

struct tempstore_t {
    std::vector<Var> store;
    tempstore_t() {
        store.push_back(Var(nullptr));
    }
    parser::ref pass(Var& v) {
        if (v->pref) return v->pref;
        store.push_back(v);
        return store.size() - 1;
    }
    parser::ref retain(const Var& v) {
        store.push_back(v);
        return store.size() - 1;
    }
    parser::ref emplace(const std::string& value, bool numeric = false) {
        return retain(std::make_shared<var_t>(value, numeric));
    }
    inline Var& pull(parser::ref r) {
        for (;;) {
            Var v = store[r];
            if (v->pref && v->pref != r) { r = v->pref; continue; }
            return store[r];
        }
    }
};

enum class layout_t {
    vertical,
    horizontal,
};

struct context_t {
    std::map<std::string,Var> vars;
    std::map<std::string,std::string> links;
    std::vector<std::string> aspects;
    layout_t layout = layout_t::vertical;
    tempstore_t temps;
};

typedef std::shared_ptr<context_t> Context;

struct env_t: public parser::st_callback_table {
    Context ctx;

    env_t() {
        ctx = std::make_shared<context_t>();
    }

    parser::ref load(const std::string& variable) override;
    void save(const std::string& variable, const Var& value);
    void save(const std::string& variable, parser::ref value) override;
    parser::ref constant(const std::string& value, parser::token_type type) override;
    parser::ref scanf(parser::ref input, const std::string& fmt, const std::vector<std::string>& varnames) override;
    void declare(const std::string& key, parser::ref value) override;
    void declare_aspects(const std::vector<std::string>& aspects) override;
    parser::ref fit(const std::vector<std::string>& sources) override;
    parser::ref key(parser::ref source) override;
};
