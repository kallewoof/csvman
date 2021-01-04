#include <stdexcept>

#include "parser.h"

namespace parser {

Token head = nullptr;

// std::string indent = "";
#define try(parser) \
    /*indent += " ";*/ \
    x = parser(cache, s); \
    /*indent = indent.substr(1);*/\
    if (x) {\
        if (cache.count(pcv)) delete cache[pcv];\
        cache[pcv] = new cache_t(x->clone(), s);\
        /*printf("#%zu [caching %s=%p(%s, %s)]\n", count(head, *s), x->to_string().c_str(), *s, *s ? token_type_str[(*s)->token] : "<null>", *s ? (*s)->value ?: "<nil>" : "<null>");*/\
        /* printf("GOT " #parser ": %s\n", x->to_string().c_str());*/\
        return x;\
    }
#define DEBUG_PARSER(s) //pdo __pdo(s) //printf("- %s\n", s)

ST parse_variable(cache_map& cache, Token& s) {
    DEBUG_PARSER("variable");
    if (s->token == tok_symbol) {
        var_t* t = new var_t(s->value);
        s = s->next;
        return t;
    }
    return nullptr;
}

ST parse_value(cache_map& cache, Token& s) {
    DEBUG_PARSER("value");
    if (s->token == tok_symbol || s->token == tok_number || s->token == tok_string) {
        value_t* t = new value_t(s->token, s->value);
        s = s->next;
        return t;
    }
    return nullptr;
}

std::vector<std::string> parse_symbol_list(cache_map& cache, Token& s) {
    // tok_symbol tok_comma ...
    DEBUG_PARSER("symbol_list");
    std::vector<std::string> values;
    Token r = s;

    while (r) {
        var_t* next = (var_t*)parse_variable(cache, r);
        if (!next) break;
        values.push_back(next->varname);
        if (!r || r->token != tok_comma) break;
        r = r->next;
    }

    if (values.size() != 0) s = r;

    return values;
}

ST parse_field(cache_map& cache, Token& s) {
    // tok_symbol "as" tok_string
    // tok_symbol "as" lcurly CSV rcurly
    DEBUG_PARSER("field");
    Token r = s;
    var_t* var = (var_t*)parse_variable(cache, r);
    if (!var) return nullptr;
    std::string field_name = var->varname;
    delete var;
    {
        var_t* as = (var_t*)parse_variable(cache, r);
        if (!as) return nullptr;
        if (!r || as->varname != "as") { delete as; return nullptr; }
        delete as;
    }
    if (r->token == tok_string) {
        // simplified case (single sscanf into field name)
        s = r;
        return new field_t(field_name, new as_t(r->value));
    }
    if (r->token != tok_lcurly || !r->next || r->next->token != tok_string || !r->next->next || r->next->next->token != tok_comma) return nullptr;
    r = r->next;
    value_t* val = (value_t*)parse_value(cache, r);
    std::string fmt = val->value;
    delete val;
    r = r->next; // we already checked tok_comma
    auto list = parse_symbol_list(cache, r);
    if (list.size() == 0) return nullptr;
    if (!r || r->token != tok_rcurly) return nullptr;
    s = r->next;
    return new field_t(field_name, new as_t(fmt, list));
}

ST parse_set(cache_map& cache, Token& s) {
    // tok_symbol tok_set [expr]
    DEBUG_PARSER("set");
    Token r = s;
    var_t* var = (var_t*)parse_variable(cache, r);
    if (!var) return nullptr;
    std::string varname = var->varname;
    delete var;
    if (!r || !r->next || r->token != tok_set) return nullptr;
    r = r->next;
    ST val = parse_expr(cache, r);
    if (!val) return nullptr;
    s = r;
    ST rv = new set_t(varname, val);
    return rv;
}

ST parse_declarations(cache_map& cache, Token& s) {
    DEBUG_PARSER("declarations");
    Token r = s;
    var_t* var = (var_t*)parse_variable(cache, r);
    if (!var) return nullptr;
    auto type = var->varname;
    delete var;

    if (type == "aspects") {
        // aspects A, B, ...
        auto aspects = parse_symbol_list(cache, r);
        if (aspects.size() < 2) return nullptr;
        return new aspects_t(aspects);
    }

    if (type == "layout") {
        var = (var_t*)parse_variable(cache, r);
        if (!var) return nullptr;
        auto rv = new decl_t(type, var);
    }

    return nullptr;
}

ST parse_expr(cache_map& cache, Token& s) {
    DEBUG_PARSER("expr");
    ST x;
    Token pcv = s;
    if (cache.count(pcv)) return cache.at(pcv)->hit(s);
    // foobar38, "foobar", 38, foobar38 = [...], layout vertical, foobar38 as "%u", foobar38 as { "%u-%u-%u", a, b, c }
    try(parse_field);
    // foobar38, "foobar", 38, foobar38 = [...], layout vertical
    try(parse_declarations);
    // foobar38, "foobar", 38, foobar38 = [...]
    try(parse_set);
    // foobar38, "foobar", 38
    try(parse_value);
    // foobar38
    try(parse_variable);
    //
    return nullptr;
}

ST parse_alloc_one(Token tokens) {
    head = tokens;
    cache_map cache;
    uint64_t flags = 0;
    // pws ws(pcache, flags);
    Token s = tokens;
    ST value = parse_expr(cache, s);
    head = nullptr;
    if (!s && value) {
        value = value->clone();
    }
    for (auto& v : cache) delete v.second;
    if (s) {
        throw std::runtime_error("failed to treeify tokens around token " + std::string(s->value ?: token_type_str[s->token]));
        return nullptr;
    }
    return value;

}

std::vector<ST> parse_alloc(Token tokens) {
    Token start = tokens;
    Token prev = nullptr;
    std::vector<ST> results;
    while (tokens) {
        if (prev && tokens->token == tok_semicolon) {
            prev->next = nullptr;
            results.push_back(parse_alloc_one(start));
            prev->next = tokens;
            start = tokens->next;
        }
        prev = tokens;
        tokens = tokens->next;
    }
    return results;
}

void parse_done(std::vector<ST>& parsed) {
    for (ST t : parsed) delete t;
}

// ST parse_csv(cache_t& cache, Token& s, token_type restricted_type) {
//     // [expr] [tok_comma [expr] [tok_comma [expr] [...]]
//     DEBUG_PARSER("csv");
//     std::vector<st_c> values;
//     Token r = s;

//     while (r) {
//         ST next;
//         switch (restricted_type) {
//         case tok_undef:  next = parse_expr(cache, r); break;
//         case tok_symbol: next = parse_variable(cache, r); break;
//         default: throw std::runtime_error(strprintf("unsupported restriction type %s", token_type_str[restricted_type]));
//         }
//         if (!next) break;
//         values.emplace_back(next);
//         if (!r || r->token != tok_comma) break;
//         r = r->next;
//     }

//     if (values.size() == 0) return nullptr;
//     s = r;

//     return new list_t(values);
// }

} // namespace parser
