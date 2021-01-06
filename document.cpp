#include "document.h"
#include "parser/tokenizer.h"
#include "parser/parser.h"
#include "parser/csv.h"

using Token = parser::Token;

FILE* fopen_or_die(const char* fname, bool reading) {
    FILE* fp = fopen(fname, reading ? "r" : "w");
    if (!fp) {
        fprintf(stderr, "failed to open file for %s: %s\n", reading ? "reading" : "writing", fname);
        exit(2);
    }
    return fp;
}

group_t group_t::iterate(size_t index, Value v) const {
    group_t g(*this);
    g.values[index] = v;
    return g;
}

group_t group_t::exclude(size_t index) const {
    group_t g(*this);
    g.values.erase(g.values.begin() + index);
    return g;
}

document_t::document_t(const char* path) {
    FILE* fp = fopen_or_die(path, fmode_reading);
    ctx = CompileCMF(fp);
    fclose(fp);
    int ki = 0;
    for (const auto& v : ctx->vars) {
        if (v.second->key) {
            key_indices[v.first] = ki++;
            keys.push_back(v.second);
        } else {
            values.push_back(v.second);
        }
    }
}

void document_t::align(const std::vector<std::string>& headers) {
    aligned.clear();
    for (size_t i = 0; i < headers.size(); ++i) {
        std::string header = headers[i];
        // printf(" aligning %s (%s)\n", header.c_str(), ctx->links.count(header) ? "present" : "absent");
        if (ctx->links.count(header)) {
            Var v = ctx->vars.at(ctx->links.at(header));
            v->index = int(i);
            aligned.push_back(v);
        } else if (ctx->trailing.get()) {
            ctx->trailing->index = i;
            trail = std::vector<std::string>(headers.begin() + i, headers.end());
            return;
        }
    }
}

void document_t::record_state(const Value& aspect_value) {
    group_t keys;

    for (const auto& v : keys) {
        keys.values.push_back(v->imprint());
    }

    if (aspect != "" && data.count(keys) > 0) {
        // insert aspect only and move on as the remaining data should be the same (and even if it isn't, this would simply overwrite it)
        data[keys][aspect] = aspect_value;
        return;
    }

    std::map<std::string,Value>& valuemap = data[keys];

    if (aspect != "") {
        valuemap[aspect] = aspect_value;
    }

    for (const auto &v : values) {
        valuemap[m.first] = v->imprint();
    }
}

void document_t::process(const std::vector<std::string>& row) {
    // update vars
    for (const auto& v : aligned) {
        v->read(row.at(v->index));
    }
    if (trail.size() == 0) {
        record_state();
        return;
    }
    // iterate
    for (size_t i = ctx->trailing->index; i < row.size(); ++i) {
        ctx->trailing->read(trail[i - ctx->trailing->index]);
        record_state(row.at(i));
    }
}

bool group_t::operator<(const group_t& other) const {
    for (size_t i = 0; i < values.size(); ++i) {
        if (*values[i] < *other.values[i]) return true;
        if (*other.values[i] < *values[i]) return false;
    }
    return false;
}

std::string group_t::to_string() const {
    std::string s = "(group) [";
    for (size_t i = 0; i < values.size(); ++i) {
        s += (i ? ", " : "") + values[i]->to_string();
    }
    return s + "]";
}

void document_t::load_single(FILE* fp) {
    csv reader(fp);
    std::vector<std::string> row;
    if (!reader.read(row)) {
        fprintf(stderr, "could not read header from CSV file\n");
        exit(4);
    }
    align(row);
    printf("Aligned vars:\n");
    for (const auto& v : ctx->vars) {
        printf("- %s = %s\n", v.first.c_str(), v.second->to_string().c_str());
    }
    size_t count = 0;
    while (reader.read(row)) {
        process(row);
        ++count;
    }
    printf("Read %zu lines (%zu entries)\n", count, data.size());
}

void document_t::load_from_disk(argiter_t& argiter) {
    if (ctx->aspects.size() > 0) {
        // aspect based which means path is multiple files
        for (size_t i = 0; i < ctx->aspects.size(); ++i) {
            aspect = ctx->aspects[i];
            load_single(fopen_or_die(argiter.next(), fmode_reading));
        }
    } else {
        load_single(fopen_or_die(argiter.next(), fmode_reading));
    }
}

// void document_t::write_state(const std::string& aspect_value) {
//     group_t keys;

//     // printf("write state:\n");
//     for (const auto& m : ctx->vars) {
//         Var v = m.second;
//         if (v->key) {
//             keys.values.push_back(v->imprint());
//             // printf("- key %s = %s\n", m.first.c_str(), keys.values.back()->to_string().c_str());
//         }
//     }
//     // printf("-> %s [count: %lu]\n", keys.to_string().c_str(), data.count(keys));

//     if (aspect != "" && data.count(keys) > 0) {
//         // insert aspect only and move on as the remaining data should be the same (and even if it isn't, this would simply overwrite it)
//         data[keys][aspect] = aspect_value;
//         return;
//     }

//     std::map<std::string,std::string>& valuemap = data[keys];

//     if (aspect != "") {
//         valuemap[aspect] = aspect_value;
//     }

//     for (const auto&m : ctx->vars) {
//         Var v = m.second;
//         if (!v->key) valuemap[m.first] = v->write();
//     }

//     // printf("=> {");
//     // for (const auto& m : valuemap) {
//     //     printf("\n\t%s = %s", m.first.c_str(), m.second.c_str());
//     // }
//     // printf(" }\n");
// }

void document_t::process(const std::map<std::string,Value>& valuemap) {
    // update vars
    for (const auto& m : ctx->vars) {
        if (valuemap.count(m.first)) {
            m.second->read(valuemap.at(m.first));
        }
    }
    if (trail.size() == 0) {
        write_state();
        return;
    }
    // iterate
    for (size_t i = ctx->trailing->index; i < row.size(); ++i) {
        ctx->trailing->read(trail[i - ctx->trailing->index]);
        write_state(row.at(i));
    }
}

void document_t::write_single(const document_t& doc, FILE* fp) {
    csv writer(fp);
    std::vector<std::string> row;
    size_t pretrail = doc.data.begin()->first.values.size() + doc.data.begin()->second.size() - ctx->aspects.size();

    row.resize(pretrail);

    // generate header and trail
    size_t i;
    for (const auto& var : aligned) {
        row[var->index] = var->str;
        ++i;
    }

    if (ctx->trailing) {
        // load each trailing entry from doc context, convert using own context into own format, and then write to trail and row
        trail.clear();
        ctx->trailing->index = row.size();
        for (const auto& entry : doc.data) {
            const auto& val = entry.first.values.at(trailpos);
            ctx->trailing->read(val);
            const auto& w = ctx->trailing->write();
            trail.push_back(w);
            row.push_back(w);
        }
    }

    writer.write(row);

    if (ctx->trailing) {
        // we have the trailing var values; we now need to iterate over the other key(s)
        // TODO: currently only supports one other key
        int idx = 0;
        int trail_idx = ctx->trailing->index;
        std::string varname;
        for (const auto& v : doc.keys) {
            if (!v->trails) {
                varname = doc.ctx->varnames[v];
                break;
            }
            ++idx;
        }
        Var key = keys[idx];
        std::set<Value> keyset;
        doc.create_index(idx, keyset, ctx->vars[varname]);

        // starting point
        group_t g(doc.data.begin()->first);
        for (const auto& v : keyset) {
            key->read(v);
            row[key->index] = key->write();
            g.values[idx] = v;
            size_t rowidx = pretrail;
            bool initial = true;
            for (const auto& t : trail) {
                ctx->trailing->read(t);
                // we are using our own imprint of the value here (e.g. 2021-01-06), but it's retaining components, so any other format should work fine
                // e.g. if the incoming doc uses "2021/01/06".
                g.values[trail_idx]->value = ctx->trailing->imprint();
                const auto& valuemap = doc.data.at(g);
                if (initial) {
                    for (const auto& m : valuemap) {
                        if (ctx->vars.count(m.first)) {
                            auto v = ctx->vars.at(m.first);
                            v->read(m.second);
                            if (!v->trails) {
                                row[v->index] = v->write();
                            }
                        }
                    }
                    initial = false;
                }
                row[rowidx++] = valuemap.at(aspect)->value; // TODO: deal with conversions
            }
            // completed one row; phew!
            writer.write(row);
        }
        return;
    }

    // the simple case: we read each entry as it comes, and writes it out to the disk ordered as described

    size_t count = 0;
    for (const auto& entry : doc.data) {
        size_t kiter = 0;
        for (const auto& m : ctx->vars) {
            Var v = m.second;
            if (v->key) {
                v->read(entry.first.values.at(kiter++));
            } else {
                v->read(entry.second[m.first]);
            }
            if (v->index > -1) row[v->index] = v->write();
        }
        writer.write(row);
        ++count;
    }
    printf("Wrote %zu lines (%zu entries)\n", count, doc.data.size());
}

void document_t::save_data_to_disk(const document_t& doc, const std::string& path) {
    // auto-align everything; since this is always the same (since we decide the order), we only do this once
    int index = 0;
    aligned.clear();
    for (const auto& v : ctx->vars) {
        if (!v.second->trails) {
            v.second->index = index++;
            aligned.push_back(v);
        }
    }
    if (ctx->trailing) ctx->trailing->index = index;
    printf("Auto-aligned vars:\n");
    for (const auto& v : ctx->vars) {
        printf("- %s = %s\n", v.first.c_str(), v.second->to_string().c_str());
    }

    if (ctx->aspects.size() > 0) {
        std::string basename = path.length > 4 && path.substr(path.length - 4) == ".csv" ? path.substr(0, path.length - 4) : path;
        for (size_t i = 0; i < ctx->aspects.size(); ++i) {
            aspect = ctx->aspects[i];
            write_single(doc, fopen_or_die((basename + "_" + aspect + ".csv").c_str(), fmode_writing));
        }
    } else {
        write_single(doc, fopen_or_die(path.c_str(), fmode_writing));
    }
}

void document_t::create_index(size_t group_index, std::set<Value>& dest, Var formatter) const {
    dest.clear();
    for (const auto& entry : data) {
        formatter->read(entry.first.values.at(group_index));
        dest.push_back(formatter->imprint());
    }
}

void document_t::transform_group(const document_t& maker, group_t& group) const {
    // TODO: optimize
    group_t cp(group);
    size_t maker_i = 0;
    for (const auto& maker_var : maker.ctx->vars) {
        if (maker_i != ctx->vars.at(maker_var.first)->)
    }
}

Context CompileCMF(FILE* fp) {
    size_t cap = 128;
    size_t rem = 128;
    size_t read;
    char* buf = (char*)malloc(cap);
    char* pos = buf;
    while (0 < (read = fread(pos, 1, rem, fp))) {
        if (read < rem) {
            break;
        }
        pos += read;
        cap <<= 1;
        read = pos - buf;
        buf = (char*)realloc(buf, cap);
        pos = buf + read;
        rem = cap - read;
    }

    Token t = parser::tokenize(buf);
    free(buf);

    std::vector<parser::ST> program = parser::parse_alloc(t);

    env_t e;

    uint32_t lineno = 0;
    for (auto& line : program) {
        // printf("%03u %s\n", ++lineno, line->to_string().c_str());
        line->eval(&e);
        delete line;
    }

    return e.ctx;
}
