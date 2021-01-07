#include <stdexcept>
#include <cstdio>

#include "document.h"
#include "parser/csv.h"
#include "utils.h"

int fake_argc = 5;
const char* fake_argv[] = {
    "./compile",
    "formulas/covid-19/ulklc.cmf",
    "./rawReport.csv",
    "formulas/covid-19/cssegi.cmf",
    "res.csv"
};

int main(int argc, const char* argv[]) {
    #define argc fake_argc
    #define argv fake_argv
    if (argc < 5) {
        fprintf(stderr, "syntax: %s <input cmf file> <input csv file*> <output cmf file> <output csv file>\n", argv[0]);
        fprintf(stderr, "for aspect-based inputs, provide one filename for each aspect, in order; aspect based outputs are written as 'name_<aspect>.csv'\n");
        return 1;
    }
    argiter_t argiter(argc, argv);

    document_t doc(argiter.next());
    doc.load_from_disk(argiter);

    document_t doc2(argiter.next());
    doc2.save_data_to_disk(doc, argiter.next());
}
