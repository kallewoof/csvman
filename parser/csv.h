#ifndef included_csv_h_
#define included_csv_h_

#include <vector>
#include <string>
#include <cstdio>

class csv {
public:
    FILE* fp;
    csv(FILE* fp_in) : fp(fp_in) {}
    ~csv() { fclose(fp); }
    bool read(std::vector<std::string>& vec);
    void write(const std::vector<std::string>& vec);
    // static void parse(const char* csv, std::vector<std::string>& vec);
};

#endif // included_csv_h_
