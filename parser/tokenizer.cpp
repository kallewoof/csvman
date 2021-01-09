// Copyright (c) 2021 Karl-Johan Alm
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdexcept>

#include "tokenizer.h"

namespace parser {

Token tokenize(const char* s) {
    Token head = nullptr;
    Token tail = nullptr;
    Token prev = nullptr;
    bool open = false;
    bool finalized = true;
    bool spaced = false;
    char finding = 0;
    size_t token_start = 0;
    size_t i;
    int consumes;
    int lineno = 1;
    int col = 0;
    for (i = 0; s[i]; ++i) {
        // if we are finding a character, keep reading until we find it
        ++col;
        if (s[i] == '\n') {
            col = 1;
            ++lineno;
        }
        if (finding) {
            if (s[i] != finding) continue;
            finding = 0;
            open = false;
            continue; // we move one extra step, or "foo" will be read in as "foo
        }
        auto token = determine_token(s[i], i ? s[i-1] : 0, spaced ? tok_ws : tail ? tail->token : tok_undef, consumes, tail ? &tail->token : nullptr);
        if (consumes) {
            // we only support 1 token consumption at this point
            tail->token = tok_consumable;
        }
        if (token == tok_comment) {
            finding = '\n';
            continue;
        }
        // printf("token = %s\n", token_type_str[token]);
        if (token == tok_consumable && tail->token == tok_consumable) {
            char buf[128];
            sprintf(buf, "tokenization failure at character '%c' (%03d:%d)", s[i], lineno, col);
            throw std::runtime_error(buf);
            delete head;
            return nullptr;
        }
        // if whitespace, close
        if (token == tok_ws) {
            open = false;
            spaced = true;
        } else {
            spaced = false;
        }
        // if open, see if it stays open
        if (open) {
            open = token == tail->token;
        }
        if (!open) {
            if (!finalized) {
                tail->value = strndup(&s[token_start], i-token_start);
                finalized = true;
            }
            switch (token) {
            case tok_comment:
                // we technically never get here
            case tok_string:
                finding = '"';
            case tok_symbol:
            case tok_number:
            case tok_minus:
            case tok_consumable:
                prev = tail;
                finalized = false;
                token_start = i;
                tail = new token_t(token, tail);
                if (!head) head = tail;
                open = true;
                break;
            case tok_set:
            case tok_equal:
            case tok_mul:
            case tok_lparen:
            case tok_rparen:
            case tok_comma:
            case tok_semicolon:
            case tok_lcurly:
            case tok_rcurly:
                if (tail && tail->token == tok_consumable) {
                    delete tail;
                    if (head == tail) head = prev;
                    tail = prev;
                }
                prev = tail;
                tail = new token_t(token, tail);
                tail->value = strndup(&s[i], 1 /* misses 1 char for concat/hex/bin, but irrelevant */);
                if (!head) head = tail;
                break;
            case tok_ws:
                break;
            case tok_undef:
                {
                    char buf[128];
                    sprintf(buf, "tokenization failure at character '%c' (%03d:%d)", s[i], lineno, col);
                    throw std::runtime_error(buf);
                }
                delete head;
                return nullptr;
            }
        }
        // for (auto x = head; x; x = x->next) printf(" %s", token_type_str[x->token]); printf("\n");
    }
    if (!finalized) {
        tail->value = strndup(&s[token_start], i-token_start);
        finalized = true;
    }
    return head;
}

} // namespace parser
