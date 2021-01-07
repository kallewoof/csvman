#ifndef included_document_h_
#define included_document_h_

#include <set>
#include <memory>

#include "env.h"
#include "utils.h"

struct group_t {
    std::vector<Value> values;
    bool operator<(const group_t& other) const;
    std::string to_string() const;
    group_t iterate(size_t index, Value v) const;
    group_t exclude(size_t index) const;
};

enum class import_mode {
    /**
     * Clear out the destination data before importing the source data, only changing formatting.
     */
    replace         = 0,
    /**
     * Replace all values in destination which also exist in source, keeping only distinct values.
     */
    merge_source    = 1,
    /**
     * Insert values not previously found, and keep existing values.
     */
    merge_dest      = 2,
    /**
     * For any values existing in both documents, (1) for numeric values, take the average of the
     * two as the resulting value; (2) for non-numeric values, keep the existing value.
     */
    merge_average   = 3,
    /**
     * Given the parameter key, (1) calculate the maximum value of key inside destination,
     * (2) merge values from source iff the value of the parameter key is greater than the
     * maximum in (1).
     */
    merge_forward   = 4,
};

class document_t;

typedef std::shared_ptr<document_t> Document;

class document_t {
public:
    std::map<group_t, std::map<std::string, Value>> data;
    std::string aspect;

    std::string cmf_path;

    document_t(Context ctx_in) : ctx(ctx_in) {}
    document_t(const char* path);
    document_t() : ctx(nullptr) {}

    // align var names to header indices in a document (e.g. a CSV file's first line)
    void align(const std::vector<std::string>& headers);

    // process and update data
    void process(const std::vector<std::string>& row);

    void record_state(const Value& aspect_value = nullptr);

    void load_from_disk(cliargs& argiter);

    void import_data(const std::vector<Document>& sources, import_mode mode = import_mode::replace, const std::string& import_param = "");

    void save_data_to_disk(const std::string& path);

    void save_data_to_disk(const document_t& doc, const std::string& path);
private:
    Context ctx;
    std::vector<Var> keys, values, aligned;
    std::map<std::string, int> key_indices;
    std::vector<std::string> trail; // for formats with a trail (header contains e.g. dates in a trail going right), this contains the header values

    void load_single(FILE* fp);
    void write_single(const document_t& doc, FILE* fp);

    void create_index(size_t group_index, std::set<val_t>& dest, Var formatter) const;
};

Context CompileCMF(FILE* fp);

constexpr bool fmode_reading = true;
constexpr bool fmode_writing = false;

FILE* fopen_or_die(const char* fname, bool reading);

inline import_mode parse_import_mode(const std::string& mode) {
    if (mode == "replace") return import_mode::replace;
    if (mode == "merge-source") return import_mode::merge_source;
    if (mode == "merge-dest") return import_mode::merge_dest;
    if (mode == "merge_average") return import_mode::merge_average;
    if (mode == "merge-forward") return import_mode::merge_forward;
    throw std::runtime_error("unknown import mode " + mode);
}

#endif // included_document_h_
