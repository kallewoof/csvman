#ifndef included_utils_h_
#define included_utils_h_

#include <stdexcept>

struct argiter_t {
    int argc, argi;
    const char** argv;
    argiter_t(int argc_in, const char* argv_in[]) : argc(argc_in), argv(argv_in), argi(1) {}
    const char* next() {
        if (argc == argi) throw std::runtime_error("argument overflow");
        return argv[argi++];
    }
    const char* last() {
        if (argi == 0) throw std::runtime_error("invalid argument operation (none fetched but requesting last fetched)");
        return argv[argi - 1];
    }
};

#endif // included_utils_h_
