#include "document.h"
#include "parser/tokenizer.h"
#include "parser/parser.h"

using Token = parser::Token;

Context CompileCMF(FILE* fp) {
    size_t cap = 128;
    size_t rem = 128;
    size_t read;
    char* buf = (char*)malloc(cap);
    char* pos = buf;
    while (0 < (read = fread(pos, 1, rem, fp))) {
        if (read < rem) {
            break;
        }
        pos += read;
        cap <<= 1;
        read = pos - buf;
        buf = (char*)realloc(buf, cap);
        pos = buf + read;
        rem = cap - read;
    }

    Token t = parser::tokenize(buf);
    delete buf;

    std::vector<parser::ST> program = parser::parse_alloc(t);

    env_t e;

    uint32_t lineno = 0;
    for (auto& line : program) {
        printf("%03u %s\n", ++lineno, line->to_string().c_str());
        line->eval(&e);
        delete line;
    }

    return e.ctx;
}
