#include "csv.h"

bool csv::read(std::vector<std::string>& vec) {
    *buf = 0;
    for (;;) {
        char* pos = fgets(buf, cap, fp);
        if (!pos || *pos == '\n' || pos == buf) break;
        cap <<= 1;
        buf = (char*)realloc(buf, cap);
    }
    if (*buf == 0) return false;

    bool quoted = false;
    vec.resize(0);
    // printf("parsing %s\n", buf);
    size_t start = 0;
    for (size_t i = 0; buf[i - (i > 0)]; ++i) {
        if (quoted) {
            if (buf[i] == '"') {
                quoted = false;
            }
        } else if (!buf[i] || buf[i] == ',' || buf[i] == '\n') {
            vec.emplace_back(&buf[start], &buf[i]);
            start = i + 1;
            // printf("emplaced %s\n", vec.back().c_str());
            if (buf[i] == '\n') return true;
        }
    }

    return true;
}
