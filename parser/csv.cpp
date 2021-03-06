#include "csv.h"

std::string csv::stringify(const std::vector<std::string>& vec) {
    std::string s = "";
    for (const auto& t : vec) {
        s += (s == "" ? "" : ",") + t;
    }
    return s;
}

// void csv::parse(const char* buf, std::vector<std::string>& vec) {
//     bool quoted = false;
//     vec.resize(0);
//     // printf("parsing %s\n", buf);
//     size_t start = 0;
//     for (size_t i = 0; buf[i - (i > 0)]; ++i) {
//         if (quoted) {
//             if (buf[i] == '"') {
//                 quoted = false;
//             }
//         } else if (!buf[i] || buf[i] == ',' || buf[i] == '\n') {
//             vec.emplace_back(&buf[start], &buf[i]);
//             start = i + 1;
//             // printf("emplaced %s\n", vec.back().c_str());
//             if (buf[i] == '\n') return;
//         }
//     }

//     return;
// }

void csv::write(const std::vector<std::string>& vec) {
    for (size_t i = 0; i < vec.size(); ++i) {
        const std::string& string = vec.at(i);
        const char* cs = string.c_str();

        if (i > 0) fputc(',', fp);

        bool quote = (string.find(',') != std::string::npos);

        if (quote) fputc('"', fp);
        while (*cs) {
            fputc(*cs, fp);
            cs++;
        }
        if (quote) fputc('"', fp);
    }
    fputc('\n', fp);
}

bool csv::read(std::vector<std::string>& vec) {
    bool quoted = false;
    vec.resize(0);
    // printf("parsing %s\n", buf);
    char ch, buf[512];
    size_t len = 0;
    short crop = 0;
    while (EOF != (ch = buf[len++] = fgetc(fp))) {
        if (quoted) {
            if (ch == '"') {
                quoted = false;
                crop = 1;
            }
        } else if (ch == ',' || ch == '\n') {
            buf[len - crop - 1] = 0;
            vec.emplace_back(&buf[crop]);
            len = 0;
            crop = 0;
            // printf("emplaced %s\n", vec.back().c_str());
            if (ch == '\n' || ch == -1) return true;
        } else if (ch == '"') {
            quoted = true;
        }
    }
    if (!ch && len > 1) {
        buf[len - crop - 1] = 0;
        vec.emplace_back(&buf[crop]);
    }

    return vec.size() > 0;
}
