#ifndef included_utils_h_
#define included_utils_h_

#include <stdexcept>

#include <map>
#include <vector>
#include <set>

#include <signal.h>
#include <cstring>
#include <getopt.h>

static inline void debugbreak() {
    raise(SIGTRAP);
}

static inline bool is_numeric(const char* value) {
    if (!*value) return false;
    bool decimal = true;
    for (; *value; ++value) {
        if (decimal && *value == '.') {
            decimal = false;
        } else if (*value < '0' || *value > '9') {
            return false;
        }
    }
    return true;
}

enum cliarg_type {
    no_arg = no_argument,
    req_arg = required_argument,
    opt_arg = optional_argument,
};

struct cliopt {
    char* longname;
    char shortname;
    cliarg_type type;
    cliopt(const char* longname_in, const char shortname_in, cliarg_type type_in)
    : longname(strdup(longname_in))
    , shortname(shortname_in)
    , type(type_in)
    {}
    ~cliopt() { free(longname); }
    struct option get_option(std::string& opt) {
        char buf[3];
        sprintf(buf, "%c%s", shortname, type == no_arg ? "" : ":");
        opt += buf;
        return {longname, type, nullptr, shortname};
    }
};

struct cliargs {
    std::map<char, std::string> m;
    std::vector<const char*> l;
    std::vector<cliopt*> long_options;
    size_t iter{0};

    ~cliargs() {
        while (!long_options.empty()) {
            delete long_options.back();
            long_options.pop_back();
        }
    }
    void add_option(const char* longname, const char shortname, cliarg_type t) {
        long_options.push_back(new cliopt(longname, shortname, t));
    }
    void parse(int argc, char* const* argv) {
        struct option long_opts[long_options.size() + 1];
        std::string opt = "";
        for (size_t i = 0; i < long_options.size(); i++) {
            long_opts[i] = long_options[i]->get_option(opt);
        }
        long_opts[long_options.size()] = {0,0,0,0};
        int c;
        int option_index = 0;
        for (;;) {
            c = getopt_long(argc, argv, opt.c_str(), long_opts, &option_index);
            if (c == -1) {
                break;
            }
            if (optarg) {
                m[c] = optarg;
            } else {
                m[c] = "1";
            }
        }
        while (optind < argc) {
            l.push_back(argv[optind++]);
        }
    }

    const char* next() {
        if (l.size() == iter) throw std::runtime_error("argument overflow");
        return l[iter++];
    }
    const char* last() {
        if (iter == 0) throw std::runtime_error("invalid argument operation (none fetched but requesting last fetched)");
        return l[iter - 1];
    }
};

#endif // included_utils_h_
