#ifndef included_ast_h_
#define included_ast_h_

#include <string>

#include "tokenizer.h"

namespace parser {

typedef size_t ref;
static const ref nullref = 0;

struct st_callback_table {
    bool ret = false;
    virtual ref  load(const std::string& variable) = 0;
    virtual void save(const std::string& variable, ref value) = 0;
    // virtual ref  bin(token_type op, ref lhs, ref rhs) = 0;
    // virtual ref  unary(token_type op, ref val) = 0;
    // virtual ref  fcall(const std::string& fname, ref args) = 0;
    // virtual ref  pcall(ref program, ref args) = 0;
    // virtual ref  preg(program_t* program) = 0;
    // virtual ref  convert(const std::string& value, token_type type) = 0;
    virtual ref  constant(const std::string& value, token_type type) = 0;
    // virtual ref  to_array(size_t count, ref* refs) = 0;
    // virtual ref  at(ref arrayref, ref indexref) = 0;
    // virtual ref  range(ref arrayref, ref startref, ref endref) = 0;
    // virtual ref  compare(ref a, ref b, token_type op) = 0;
    // virtual bool truthy(ref v) = 0;
    virtual ref scanf(const std::string& input, const std::string& fmt, const std::vector<std::string>& varnames) = 0;
    virtual void declare(const std::string& key, const std::string& value) = 0;
    virtual void declare_aspects(const std::vector<std::string>& aspects) = 0;
    virtual ref fit(const std::vector<std::string>& sources) = 0;
    virtual ref key(ref source) = 0;
};

typedef struct st_t * ST;

struct st_t {
    virtual std::string to_string() {
        return "????";
    }
    virtual void print() {
        printf("%s", to_string().c_str());
    }
    virtual ref eval(st_callback_table* ct) {
        return nullref;
    }
    virtual ST clone() {
        return new st_t();
    }
};

struct st_c {
    ST r;
    size_t* refcnt;
    // void alive() { printf("made st_c with ptr %p ref %zu (%p)\n", r, refcnt ? *refcnt : 0, refcnt); }
    // void dead() { printf("deleting st_c with ptr %p ref %zu (%p)\n", r, refcnt ? *refcnt : 0, refcnt); }
    st_c(ST r_in) {
        r = r_in;
        refcnt = (size_t*)malloc(sizeof(size_t));
        *refcnt = 1;
        // alive();
    }
    st_c(const st_c& o) {
        r = o.r;
        refcnt = o.refcnt;
        (*refcnt)++;
        // alive();
    }
    st_c(st_c&& o) {
        r = o.r;
        refcnt = o.refcnt;
        o.r = nullptr;
        o.refcnt = nullptr;
        // alive();
    }
    st_c& operator=(const st_c& o) {
        if (refcnt) {
            if (!--(*refcnt)) {
                // dead();
                delete r;
                delete refcnt;
            }
        }
        r = o.r;
        refcnt = o.refcnt;
        (*refcnt)++;
        // alive();
        return *this;
    }
    ~st_c() {
        // dead();
        if (!refcnt) return;
        if (!--(*refcnt)) {
            delete r;
            delete refcnt;
        }
    }
    st_c clone() {
        return st_c(r->clone());
    }
};

struct var_t: public st_t {
    std::string varname;
    var_t(const std::string& varname_in) : varname(varname_in) {}
    virtual std::string to_string() override {
        return std::string("'") + varname;
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->load(varname);
    }
    virtual ST clone() override {
        return new var_t(varname);
    }
};

struct fit_t: public st_t {
    std::vector<std::string> references;
    fit_t(const std::vector<std::string>& references_in) : references(references_in) {}
    virtual std::string to_string() override {
        std::string s = "fit [" + std::to_string(references.size()) + " items]";
        return s;
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->fit(references);
    }
    virtual ST clone() override {
        return new fit_t(references);
    }
};

struct key_t: public st_t {
    st_c value;
    key_t(st_c value_in) : value(value_in) {}
    virtual std::string to_string() override {
        return "key " + value.r->to_string();
    }
    virtual ref eval(st_callback_table* ct) override {
        ref result = value.r->eval(ct);
        return ct->key(result);
    }
    virtual ST clone() override {
        return new key_t(value.clone());
    }
};

struct value_t: public st_t {
    token_type type; // tok_number, tok_string, tok_symbol
    std::string value;
    value_t(token_type type_in, const std::string& value_in) : type(type_in), value(value_in) {
        if (type == tok_string && value.length() > 0 && value[0] == '"' && value[value.length()-1] == '"') {
            // get rid of quotes
            value = value.substr(1, value.length() - 2);
        }
    }
    virtual std::string to_string() override {
        return value;
    }
    virtual ref eval(st_callback_table* ct) override {
        if (type == tok_symbol) return ct->load(value);
        return ct->constant(value, type);
    }
    virtual ST clone() override {
        return new value_t(type, value);
    }
};

struct set_t: public st_t {
    std::string varname;
    st_c value;
    set_t(const std::string& varname_in, st_c value_in) : varname(varname_in), value(value_in) {}
    virtual std::string to_string() override {
        return "'" + varname + " = " + value.r->to_string();
    }
    virtual ref eval(st_callback_table* ct) override {
        ref result = value.r->eval(ct);
        ct->save(varname, result);
        return result;
    }
    virtual ST clone() override {
        return new set_t(varname, value.clone());
    }
};

struct decl_t: public st_t {
    std::string key, value;
    decl_t(const std::string& key_in, const std::string& value_in) : key(key_in), value(value_in) {}
    virtual std::string to_string() override {
        return key + " " + value;
    }
    virtual ref eval(st_callback_table* ct) override {
        ct->declare(key, value);
        return nullref;
    }
    virtual ST clone() override {
        return new decl_t(key, value);
    }
};

struct aspects_t: public st_t {
    std::vector<std::string> aspects;
    aspects_t(const std::vector<std::string>& aspects_in) : aspects(aspects_in) {}
    virtual std::string to_string() override {
        std::string s = "aspects ";
        for (size_t i = 0; i < aspects.size(); ++i) {
            s += (i ? ", " : "") + aspects[i];
        }
        return s;
    }
    virtual ref eval(st_callback_table* ct) override {
        ct->declare_aspects(aspects);
        return nullref;
    }
    virtual ST clone() override {
        return new aspects_t(aspects);
    }
};

// as { "%u/%u/%u", year, month, day };
struct as_t: public st_t {
    std::string fmt;
    std::vector<std::string> fields;
    as_t(const std::string& fmt_in, const std::vector<std::string>& fields_in) : fmt(fmt_in), fields(fields_in) {}
    virtual std::string to_string() override {
        if (fields.size() == 0) return "as \"" + fmt + "\"";
        std::string s = "as { \"" + fmt + "\", ";
        for (size_t i = 0; i < fields.size(); ++i) {
            s += (i ? ", " : "") + fields[i];
        }
        return s + " }";
    }
    virtual ST clone() override {
        return new as_t(fmt, fields);
    }
};

// * as { "%u/%u/%u", year, month, day };
struct field_t: public st_t {
    std::string value;
    as_t* as;
    field_t(const std::string& value_in, as_t* as_in) : value(value_in), as(as_in) {}
    ~field_t() {
        if (as) delete as;
    }
    virtual std::string to_string() override {
        return value + (as ? " " + as->to_string() : "");
    }
    virtual ref eval(st_callback_table* ct) override {
        return ct->scanf(value, as->fmt, as->fields);
    }
    virtual ST clone() override {
        return new field_t(value, (as_t*) as->clone());
    }
};

} // namespace parser

#endif // included_ast_h_
