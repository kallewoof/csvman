#include <stdexcept>
#include <cstdio>

#include "document.h"

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "syntax: %s <cmf file>\n", argv[0]);
        return 1;
    }
    FILE* fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "failed to open file: %s\n", argv[1]);
        return 2;
    }
    Context ctx;
    try {
        ctx = CompileCMF(fp);
    } catch (const std::runtime_error& err) {
        fclose(fp);
        fprintf(stderr, "compilation error: %s\n", err.what());
        return 3;
    }
    fclose(fp);
    printf("Compilation successful\n");
    printf("Layout: %s\n", ctx->layout == layout_t::vertical ? "vertical" : "horizontal");
    printf("Vars:\n");
    for (const auto& v : ctx->vars) {
        printf("- %s = %s\n", v.first.c_str(), v.second->to_string().c_str());
    }
    printf("Links:\n");
    for (const auto& v : ctx->links) {
        printf("- %s => %s\n", v.first.c_str(), v.second.c_str());
    }
    printf("Aspects:\n");
    for (const auto& v : ctx->aspects) {
        printf("- %s\n", v.c_str());
    }
    printf("Temps:\n");
    for (const auto& v : ctx->temps.store) {
        printf("- %s\n", v->to_string().c_str());
    }
}
