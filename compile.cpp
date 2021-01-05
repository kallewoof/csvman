#include <stdexcept>
#include <cstdio>

#include "document.h"
#include "parser/csv.h"

int main(int argc, const char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "syntax: %s <input cmf file> <input csv file> <output cmf file> <output csv file>\n", argv[0]);
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
        fprintf(stderr, "compilation error in %s: %s\n", argv[1], err.what());
        return 3;
    }
    fclose(fp);
    fp = fopen(argv[3], "r");
    if (!fp) {
        fprintf(stderr, "failed to open file: %s\n", argv[3]);
        return 2;
    }
    Context ctx2;
    try {
        ctx2 = CompileCMF(fp);
    } catch (const std::runtime_error& err) {
        fclose(fp);
        fprintf(stderr, "compilation error in %s: %s\n", argv[3], err.what());
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
        printf("- %s\n", v.get() ? v->to_string().c_str() : "null");
    }
    document_t doc(ctx);
    fp = fopen(argv[2], "r");
    if (!fp) {
        fprintf(stderr, "failed to open CSV file: %s\n", argv[2]);
    }
    csv reader(fp);
    std::vector<std::string> row;
    if (!reader.read(row)) {
        fprintf(stderr, "could not read header from CSV file\n");
    }
    doc.align(row);
    printf("Aligned vars:\n");
    for (const auto& v : ctx->vars) {
        printf("- %s = %s\n", v.first.c_str(), v.second->to_string().c_str());
    }
    while (reader.read(row)) {
        doc.process(row);
    }
    fp = fopen(argv[4], "w");
    if (!fp) {
        fprintf(stderr, "could not open file for writing: %s\n", argv[4]);
    }
}
