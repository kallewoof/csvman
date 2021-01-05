#ifndef included_document_h_
#define included_document_h_

#include <memory>

#include "env.h"

struct document_t {
    Context ctx;
    // align var names to header indices in a document (e.g. a CSV file's first line)
    void align(const std::vector<std::string>& headers);
    
};

Context CompileCMF(FILE* fp);

#endif // included_document_h_
