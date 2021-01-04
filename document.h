#ifndef included_document_h_
#define included_document_h_

#include <memory>

#include "env.h"

typedef std::shared_ptr<env_t> Env;

struct document_t {
    Env env;
    // align var names to indices in a document (e.g. a CSV file)
    void align(const std::vector<std::string>& headers);
};

Env CompileCMF(FILE* fp);

#endif // included_document_h_
