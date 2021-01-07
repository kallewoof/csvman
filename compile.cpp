#include <stdexcept>
#include <cstdio>

#include "document.h"
#include "parser/csv.h"
#include "utils.h"

int fake_argc = 5;
char * const fake_argv[] = {
    "./compile",
    "formulas/covid-19/ulklc.cmf",
    "./rawReport.csv",
    "formulas/covid-19/cssegi.cmf",
    "res.csv"
};

int main(int argc, char* const* argv) {
    // #define argc fake_argc
    // #define argv fake_argv

    cliargs ca;
    ca.add_option("mode", 'm', req_arg);
    ca.add_option("param", 'p', req_arg);
    ca.add_option("output", 'o', req_arg);
    ca.add_option("help", 'h', no_arg);
    ca.add_option("verbose", 'v', no_arg);
    ca.add_option("debug", 'D', req_arg);
    ca.parse(argc, argv);
    if (ca.m.count('h') || ca.l.size() < 2) {
        fprintf(stderr, "Syntax: %s [--mode=<mode>|-m<mode> [--param=<param>|-p<param>]] <cmf file> <csv file*> [<cmf file 2> <csv file 2*> [...]] [-o <cmf file> <csv base filename>]\n", argv[0]);
        fprintf(stderr, "For aspect-based inputs, provide one filename for each aspect, in order; aspect based outputs are written as 'name_<aspect>.csv'\n");
        fprintf(stderr, "If no output (-o or --output) is provided, the last given specification is used to write the result to 'result[...].csv'.\n");
        fprintf(stderr, "For 2 documents, the first is the source, and the second is the destination (going into the third, -o CSV file).\n");
        fprintf(stderr, "For 3 or more documents, all documents except the last one are considered sources, and the last document is the destination.\n");
        fprintf(stderr, "Inputs are never overwritten, unless they are specifically given as the output using -o.\n");
        fprintf(stderr, "Modes:\n");
        fprintf(stderr, "  replace          Rewrite a single document using a different format\n");
        fprintf(stderr, "  merge-source     Replace all values in destination which also exist in source(s), keeping only distinct values.\n");
        fprintf(stderr, "  merge-dest       Insert values not previously found, and keep existing values.\n");
        fprintf(stderr, "  merge-avg        For any values existing in 2+ documents, (1) for numeric values, take the average of all values as resulting value,\n");
        fprintf(stderr, "                   (2) for non-numeric values, count the occurrences and use the most frequent, or existing if all equally frequent)\n");
        fprintf(stderr, "  merge-forward    Given the parameter key <param>, (1) calculate the maximum value of key inside destination");
        exit(1);
    }

    import_mode mode;
    if (ca.m.count('m')) {
        mode = parse_import_mode(ca.m['m']);
    }
    std::string param;
    if (ca.m.count('p')) {
        param = ca.m['p'];
    }

    size_t source_end = ca.l.size();
    Document dest;
    std::string output_path = "result.csv";
    if (ca.m.count('o')) {
        // file output, with format specified in 'o' and the file as the last argument in 'l'.
        --source_end;
        dest = std::make_shared<document_t>(ca.m.at('o').c_str());
        output_path = ca.l.back();
    }

    std::vector<Document> sources;

    while (ca.iter < source_end) {
        sources.push_back(std::make_shared<document_t>(ca.next()));
        sources.back()->load_from_disk(ca);
    }

    if (!dest) {
        dest = std::make_shared<document_t>(sources.back()->cmf_path.c_str());
    }

    dest->import_data(sources, mode, param);

    dest->save_data_to_disk(output_path);

    // document_t doc(ca.next());
    // doc.load_from_disk(ca);

    // document_t doc2(ca.next());
    // doc2.save_data_to_disk(doc, ca.next());
}
