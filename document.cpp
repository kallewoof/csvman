#include "document.h"
#include "tokenizer.h"
#include "parser.h"

using Token = parser::Token;

Env CompileCMF(FILE* fp) {
    size_t cap = 1024;
    size_t pos = 0;
    size_t read;
    char* buf = (char*)malloc(cap);
    while (0 < (read = fread(&buf[cap], 1, cap, fp))) {
        if (read < cap) {
            break;
        }
        pos += read;
        buf = (char*)realloc(buf, pos + cap);
    }

    Token t = parser::tokenize(buf);
    delete buf;

    std::vector<parser::ST> program = parser::parse_alloc(t);

    Env e = std::make_shared<env_t>();

    for (auto& line : program) {
        line->eval(e.get());
        delete line;
    }

    return e;
}
