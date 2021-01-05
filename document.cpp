#include "document.h"
#include "parser/tokenizer.h"
#include "parser/parser.h"

using Token = parser::Token;

void document_t::align(const std::vector<std::string>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
        std::string header = headers[i];
        // printf(" aligning %s (%s)\n", header.c_str(), ctx->links.count(header) ? "present" : "absent");
        if (ctx->links.count(header)) {
            Var v = ctx->vars.at(ctx->links.at(header));
            v->index = int(i);
        } else if (ctx->trailing.get()) {
            ctx->trailing->index = i;
            trail = std::vector<std::string>(headers.begin() + i, headers.end());
            return;
        }
    }
}

void document_t::write_state(const std::string& aspect_value) {
    group_t keys;

    // printf("write state:\n");
    for (const auto& m : ctx->vars) {
        Var v = m.second;
        if (v->key) {
            keys.values.push_back(v->imprint());
            // printf("- key %s = %s\n", m.first.c_str(), keys.values.back()->to_string().c_str());
        }
    }
    // printf("-> %s [count: %lu]\n", keys.to_string().c_str(), data.count(keys));

    std::map<std::string,std::string>& valuemap = data[keys];

    if (aspect != "") {
        valuemap[aspect] = aspect_value;
    }

    for (const auto&m : ctx->vars) {
        Var v = m.second;
        if (!v->key) valuemap[m.first] = v->write();
    }

    // printf("=> {");
    // for (const auto& m : valuemap) {
    //     printf("\n\t%s = %s", m.first.c_str(), m.second.c_str());
    // }
    // printf(" }\n");
}

void document_t::process(const std::vector<std::string>& row) {
    // update vars
    for (const auto& m : ctx->vars) {
        Var v = m.second;
        if (v->index != -1) v->read(row.at(v->index));
    }
    if (trail.size() == 0) {
        write_state();
        return;
    }
    // iterate
    for (size_t i = ctx->trailing->index; i < row.size(); ++i) {
        ctx->trailing->read(trail[i - ctx->trailing->index]);
        write_state(row.at(i));
    }
}

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
    free(buf);

    std::vector<parser::ST> program = parser::parse_alloc(t);

    env_t e;

    uint32_t lineno = 0;
    for (auto& line : program) {
        // printf("%03u %s\n", ++lineno, line->to_string().c_str());
        line->eval(&e);
        delete line;
    }

    return e.ctx;
}

bool group_t::operator<(const group_t& other) const {
    for (size_t i = 0; i < values.size(); ++i) {
        if (*values[i] < *other.values[i]) return true;
        if (*other.values[i] < *values[i]) return false;
    }
    return false;
}

std::string group_t::to_string() const {
    std::string s = "(group) [";
    for (size_t i = 0; i < values.size(); ++i) {
        s += (i ? ", " : "") + values[i]->to_string();
    }
    return s + "]";
}
