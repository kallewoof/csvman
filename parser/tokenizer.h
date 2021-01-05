// Copyright (c) 2021 Karl-Johan Alm
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef included_tokenizer_h_
#define included_tokenizer_h_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

namespace parser {

enum token_type {
    tok_undef,
    tok_symbol,    // variable, function name, ...
    tok_number,
    tok_set,
    tok_lparen,
    tok_rparen,
    tok_string,
    tok_comma,
    tok_semicolon,
    tok_lcurly,
    tok_rcurly,
    tok_mul,
    tok_consumable, // consumed by fulfilled token sequences
    tok_ws,
    tok_comment
};

static const char* token_type_str[] = {
    "???",
    "symbol",
    "number",
    "equal",
    "lparen",
    "rparen",
    "string",
    ",",
    ";",
    "lcurly",
    "rcurly",
    "*",
    "consumable",
    "ws",
    "comment"
};

typedef struct token_t * Token;

struct token_t {
    token_type token = tok_undef;
    char* value = nullptr;
    Token next = nullptr;
    token_t(token_type token_in, Token prev) : token(token_in) {
        if (prev) prev->next = this;
    }
    token_t(token_type token_in, const char* value_in, Token prev) :
    token_t(token_in, prev) {
        value = strdup(value_in);
    }
    ~token_t() { if (value) free(value); if (next) delete next; }
    void print() {
        printf("[%s %s]\n", token_type_str[token], value ?: "<null>");
        if (next) next->print();
    }
};

inline token_type determine_token(const char c, const char p, token_type current, int& consumes) {
    consumes = 0;
    if (c == '#') return tok_comment;
    if (c == '*') return tok_mul;
    if (c == ',') return tok_comma;
    if (c == ';') return tok_semicolon;
    if (c == '=') return tok_set;
    if (c == ')') return tok_rparen;
    if (c == '}') return tok_rcurly;
    if (c == ' ' || c == '\t' || c == '\n') return tok_ws;

    if (c >= '0' && c <= '9') return current == tok_symbol ? tok_symbol : tok_number;
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') return tok_symbol;
    if (c == '"') return tok_string;
    if (c == '(') return tok_lparen;
    if (c == '{') return tok_lcurly;
    return tok_undef;
}

Token tokenize(const char* s);

} // namespace parser

#endif // included_tokenizer_h_
