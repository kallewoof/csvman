#ifndef included_csv_h_
#define included_csv_h_

#include <vector>
#include <string>
#include <cstdio>

class csv {
public:
    FILE* fp;
    csv(FILE* fp_in) : fp(fp_in), buf((char*)malloc(128)), cap(128) {}
    ~csv() { fclose(fp); }
    bool read(std::vector<std::string>& vec);
private:
    char* buf;
    size_t cap;
};

#endif // included_csv_h_
