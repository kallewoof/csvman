#include <map>
#include <memory>

#include "parser.h"

struct var_t {
    std::string str;
    std::map<std::string, std::string> comps;

    parser::ref pref = 0;
    std::string fmt;
    std::vector<std::string> varnames;
    var_t(const std::string& str_in = "") : str(str_in) {}
    void read(const std::string& input);
    std::string write();
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
    parser::ref emplace(const std::string& value) {
        return retain(std::make_shared<var_t>(value));
    }
    inline Var& pull(parser::ref r) {
        for (;;) {
            Var v = store[r];
            if (v->pref && v->pref != r) { r = v->pref; continue; }
            return store[r];
        }
    }
};

struct context_t {
    std::map<std::string,Var> vars;
    std::vector<std::string> aspects;
    tempstore_t temps;
};

struct env_t: public parser::st_callback_table {
    context_t* ctx;
    std::vector<context_t> contexts;

    env_t() {
        contexts.resize(1);
        ctx = &contexts[0];
    }

    parser::ref load(const std::string& variable) override;
    void save(const std::string& variable, const Var& value);
    void save(const std::string& variable, parser::ref value) override;
    parser::ref convert(const std::string& value, parser::token_type type) override;
    parser::ref scanf(parser::ref input, const std::string& fmt, const std::vector<std::string>& varnames) override;
    void declare(const std::string& key, parser::ref value) override;
    parser::ref declare_aspects(const std::vector<std::string>& aspects) override;
};
