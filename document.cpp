#include "document.h"
#include "parser/tokenizer.h"
#include "parser/parser.h"
#include "parser/csv.h"
#include <assert.h>

using Token = parser::Token;

static bool verified = false;
static void verify() {
    var_t date;

    date.str = "*";
    date.value = "2021-01-05";
    date.comps["year"] = "2021";
    date.comps["month"] = "01";
    date.comps["day"] = "05";
    date.fmt = "%u-%u-%u";
    date.varnames.push_back(prioritized_t("year", 0));
    date.varnames.push_back(prioritized_t("month", 1));
    date.varnames.push_back(prioritized_t("day", 2));

    var_t date2(date);
    date2.value = "2021-01-06";
    date2.comps["day"] = "06";

    group_t g;
    g.values.push_back(date.imprint());
    group_t g2;
    g2.values.push_back(date2.imprint());
    assert (g<g2);
    assert (!(g2<g));

    verified = true;
}

FILE* fopen_or_die(const char* fname, bool reading) {
    FILE* fp = fopen(fname, reading ? "r" : "w");
    if (!fp) {
        fprintf(stderr, "failed to open file for %s: %s\n", reading ? "reading" : "writing", fname);
        exit(2);
    }
    return fp;
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
    if (!verified) verify();
    cmf_path = path;
    FILE* fp = fopen_or_die(path, fmode_reading);
    ctx = CompileCMF(fp);
    fclose(fp);
    int ki = 0;
    std::set<std::string> order;
    for (const auto& s : ctx->vars) order.insert(s.first);
    for (const auto& vn : order) {
        const auto& v = ctx->vars.at(vn);
        if (v->key) {
            key_indices[vn] = ki++;
            keys.push_back(v);
        } else {
            values.push_back(v);
            if (v->aggregates) aggregates.push_back(v);
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
    group_t gk;

    for (const auto& v : keys) {
        gk.values.push_back(v->imprint());
    }

    bool existed = data.count(gk) > 0;
    if (aspect != "" && existed && aggregates.size() == 0) {
        // insert aspect only and move on as the remaining data should be the same (and even if it isn't, this would simply overwrite it)
        data[gk][aspect] = aspect_value;
        return;
    }

    std::map<std::string,Value>& valuemap = data[gk];

    if (aspect != "") {
        valuemap[aspect] = aspect_value;
    }

    if (existed) {
        // aggregate
        for (const auto &v : aggregates) {
            valuemap[ctx->varnames[v]]->value = std::to_string(valuemap[ctx->varnames[v]]->int64() + v->imprint()->int64());
        }
    } else {
        for (const auto &v : values) {
            valuemap[ctx->varnames[v]] = v->imprint();
        }
    }
}

void document_t::process(const std::vector<std::string>& row) {
    // update vars
    for (const auto& v : aligned) {
        v->read(row.at(v->index));
    }
    if (trail.size() == 0) {
        record_state(ctx->aspect_source != "" ? ctx->vars[ctx->aspect_source]->imprint() : nullptr);
        return;
    }
    // iterate
    for (size_t i = ctx->trailing->index; i < row.size(); ++i) {
        ctx->trailing->read(trail[i - ctx->trailing->index]);
        Value v = std::make_shared<val_t>();
        v->value = row.at(i);
        record_state(v);
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

void document_t::load_from_disk(cliargs& argiter) {
    if (ctx->aspects.size() > 0) {
        // aspect based which means path is multiple files
        for (size_t i = 0; i < ctx->aspects.size(); ++i) {
            if (ctx->aspects[i].priority == -1) continue;
            aspect = ctx->aspects[i].label;
            load_single(fopen_or_die(argiter.next(), fmode_reading));
        }
    } else {
        load_single(fopen_or_die(argiter.next(), fmode_reading));
    }
}

void document_t::write_single(const document_t& doc, FILE* fp) {
    csv writer(fp);
    std::vector<std::string> row;
    size_t pretrail = keys.size() + values.size() - (ctx->trailing ? 1 : 0);

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
        size_t trailpos = doc.key_indices.at(ctx->varnames[ctx->trailing]);
        std::set<val_t> tpset;
        doc.create_index(trailpos, tpset, ctx->trailing);
        for (const auto& v : tpset) {
            ctx->trailing->read(v);
            const auto& w = ctx->trailing->write();
            trail.push_back(w);
            row.push_back(w);
        }
    }

    writer.write(row);

    size_t count = 0;

    if (ctx->trailing) {
        // we have the trailing var values; we now need to iterate over the other key(s)
        // TODO: currently only supports one other key
        // Note: the keys array is ordered alphabetically by the variable name. Since varnames are contextual, and since the specification requires
        // that all keys must exist in both documents and be keys, the keys vector is assumed identical between the two documents.
        // This, idx obtained from self here, is later used in doc.keys.
        int idx = 0;
        std::string varname;
        Var key;
        for (const auto& v : keys) {
            if (!v->trails) {
                key = v;
                varname = ctx->varnames[v];
                break;
            }
            ++idx;
        }
        std::set<val_t> keyset;
        doc.create_index(idx, keyset, key); // note: idx picked from *this, but used in doc; this is acceptable

        int trail_idx = key_indices[ctx->varnames[ctx->trailing]];

        // starting point
        group_t g(doc.data.begin()->first);
        for (const auto& v : keyset) {
            key->read(v);
            row[key->index] = key->write();
            g.values[idx] = key->imprint();
            size_t rowidx = pretrail;
            bool initial = true;
            for (const auto& t : trail) {
                ctx->trailing->read(t);
                // we are using our own imprint of the value here (e.g. 2021-01-06), but it's retaining components, so any other format should work fine
                // e.g. if the incoming doc uses "2021/01/06".
                g.values[trail_idx] = ctx->trailing->imprint();
                const auto& valuemap = doc.data.at(g);
                if (initial) {
                    for (const auto& m : valuemap) {
                        if (ctx->vars.count(m.first)) {
                            auto v = ctx->vars.at(m.first);
                            v->read(*m.second);
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
            ++count;
        }
        printf("Wrote %zu lines (%zu entries)\n", count, doc.data.size());
        return;
    }

    // the simple case: we read each entry as it comes, and writes it out to the disk ordered as described

    for (const auto& entry : doc.data) {
        size_t kiter = 0;
        for (const auto& m : ctx->vars) {
            Var v = m.second;
            if (v->key) {
                v->read(*entry.first.values.at(kiter++));
            } else {
                v->read(*entry.second.at(m.first));
            }
            if (v->index > -1) row[v->index] = v->write();
        }
        writer.write(row);
        ++count;
    }
    printf("Wrote %zu lines (%zu entries)\n", count, doc.data.size());
}

void document_t::save_data_to_disk(const document_t& doc, const std::string& path) {
    // align based on context list
    int index = 0;
    aligned.clear();
    for (const auto& v : ctx->varlist) {
        if (!v->trails) {
            v->index = index++;
            aligned.push_back(v);
        }
    }
    if (ctx->trailing) ctx->trailing->index = index;
    printf("Context-aligned vars:\n");
    for (const auto& v : ctx->vars) {
        printf("- %s = %s\n", v.first.c_str(), v.second->to_string().c_str());
    }

    if (ctx->aspects.size() > 0) {
        std::string basename = path.length() > 4 && path.substr(path.length() - 4) == ".csv" ? path.substr(0, path.length() - 4) : path;
        for (size_t i = 0; i < ctx->aspects.size(); ++i) {
            if (ctx->aspects[i].priority == -1) continue;
            aspect = ctx->aspects[i].label;
            write_single(doc, fopen_or_die((basename + "_" + aspect + ".csv").c_str(), fmode_writing));
        }
    } else {
        write_single(doc, fopen_or_die(path.c_str(), fmode_writing));
    }
}

void document_t::save_data_to_disk(const std::string& path) {
    save_data_to_disk(*this, path);
}

void document_t::create_index(size_t group_index, std::set<val_t>& dest, Var formatter) const {
    dest.clear();
    for (const auto& entry : data) {
        formatter->read(*entry.first.values.at(group_index));
        dest.insert(*formatter->imprint());
    }
}

void document_t::import_data(const std::vector<Document>& sources, import_mode mode, const std::string& import_param) {
    switch (mode) {
    case import_mode::replace:
        if (sources.size() != 1) throw std::runtime_error("replace mode only works with single sources");
        data = sources.back()->data;
        return;
    case import_mode::merge_source:
        // Replace all values in destination which also exist in source, keeping only distinct values.
        {
            for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
                Document d = *it;
                for (const auto& k : d->data) {
                    data[k.first] = k.second;
                }
            }
        }
        break;
    case import_mode::merge_dest:
        // Insert values not previously found, and keep existing values.
        {
            std::set<group_t> known;
            for (const auto& k : data) {
                known.emplace(k.first);
            }
            for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
                Document d = *it;
                for (const auto& k : d->data) {
                    if (!known.count(k.first)) {
                        known.insert(k.first);
                        data[k.first] = k.second;
                    }
                }
            }
        }
        break;
    case import_mode::merge_average:
        // For any values existing in both documents, (1) for numeric values, take the average of the
        // two as the resulting value; (2) for non-numeric values, keep the existing value.
        {
            size_t divisor = sources.size();
            std::set<std::string> numeric;
            for (const auto& m : sources[0]->data.begin()->second) {
                if (m.second->number) numeric.insert(m.first);
            }
            for (const auto& doc : sources) {
                for (const auto& m : doc->data.begin()->second) {
                    if (!m.second->number && numeric.count(m.first)) {
                        numeric.erase(m.first);
                    }
                }
            }

            std::set<group_t> known;
            for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
                Document d = *it;
                for (const auto& k : d->data) {
                    if (!known.count(k.first)) {
                        data[k.first] = k.second;
                        std::map<std::string, Value>& vm = data[k.first];
                        for (const std::string& num : numeric) {
                            // only support integers atm
                            int64_t i = 0;
                            for (const auto& d : sources) {
                                i += d->data.at(k.first).at(num)->int64();
                            }
                            i /= divisor;
                            vm[num]->value = std::to_string(i);
                        }
                        data[k.first] = k.second;
                        known.insert(k.first);
                    }
                }
            }
        }
        break;
    case import_mode::merge_forward:
        // Given the parameter key, (1) calculate the maximum value of key inside destination,
        // (2) merge values from source iff the value of the parameter key is greater than the
        // maximum in (1).
        // For 3+ documents, we assume this extends s.t. the first source appends the second,
        // and then the 3rd, and so on, each capping the minimum value.
        {
            if (import_param == "") throw std::runtime_error("import param required for merge forward mode (it is the key parameter name to use)");
            if (sources.back()->key_indices.count(import_param) == 0) throw std::runtime_error("invalid import parameter (key not found in source(s))");
            size_t param_indice = sources.back()->key_indices[import_param];
            data = sources[0]->data;
            val_t highest = *data.begin()->first.values[param_indice];
            for (const auto& k : data) {
                const auto& v = k.first.values.at(param_indice);
                if (highest < *v) highest = *v;
            }
            // now iterate in order and update the "highest" criteria for each new doc
            for (auto it = sources.begin() + 1; it != sources.end(); ++it) {
                Document doc = *it;
                val_t next = highest;
                for (const auto& k : doc->data) {
                    const auto& v = k.first.values.at(param_indice);
                    if (highest < *v) {
                        data[k.first] = k.second;
                        if (next < *v) next = *v;
                    }
                }
                highest = next;
            }
        }
        break;
    default: throw std::runtime_error("unknown import mode");
    }
}
