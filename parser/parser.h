// Copyright (c) 2021 Karl-Johan Alm
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef included_parser_h_
#define included_parser_h_

#include <map>

#include "tokenizer.h"
#include "ast.h"

namespace parser {

typedef struct cache_t * Cache;

struct cache_t {
    ST val;
    Token dst;
    cache_t(ST val_in, Token dst_in) : val(val_in), dst(dst_in) {}
    ~cache_t() {
        delete val;
    }
    ST hit(Token& s) {
        s = dst;
        return val->clone();
    }
};

typedef std::map<Token,Cache> cache_map;

ST parse_alloc_one(Token tokens);
std::vector<ST> parse_alloc(Token tokens);

void parse_done(std::vector<ST>& parsed);

ST parse_expr(cache_map& cache, Token& s);

ST parse_variable(cache_map& cache, Token& s);
ST parse_value(cache_map& cache, Token& s);
ST parse_field(cache_map& cache, Token& s);
ST parse_set(cache_map& cache, Token& s);
ST parse_declarations(cache_map& cache, Token& s);
ST parse_fit(cache_map& cache, Token& s);
ST parse_key(cache_map& cache, Token& s);

} // namespace parser

#endif // included_parser_h_
