#ifndef included_document_h_
#define included_document_h_

#include <memory>

#include "env.h"

struct group_t {
    std::vector<Value> values;
    bool operator<(const group_t& other) const;
    std::string to_string() const;
};

struct document_t {
    Context ctx;
    std::vector<std::string> trail; // for formats with a trail (header contains e.g. dates in a trail going right), this contains the header values
    std::map<group_t, std::map<std::string,std::string>> data;
    std::string aspect;

    document_t(Context ctx_in) : ctx(ctx_in) {}

    // align var names to header indices in a document (e.g. a CSV file's first line)
    void align(const std::vector<std::string>& headers);

    // process and update data
    void process(const std::vector<std::string>& row);

    void write_state(const std::string& aspect_value = "");
};

Context CompileCMF(FILE* fp);

#endif // included_document_h_
