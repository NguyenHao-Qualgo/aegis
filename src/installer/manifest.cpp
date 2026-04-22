#include "aegis/installer/manifest.hpp"

#include <fstream>
#include <regex>

namespace aegis {

namespace {

// Finds "key: ( ... )" and returns content inside the parentheses.
std::string find_list_block(const std::string &text, const std::string &key) {
    const std::string needle = key + ":";
    const std::size_t begin = text.find(needle);
    if (begin == std::string::npos) { return {}; }
    const std::size_t list_start = text.find('(', begin);
    if (list_start == std::string::npos) { return {}; }
    int depth = 0;
    for (std::size_t i = list_start; i < text.size(); ++i) {
        if      (text[i] == '(') { ++depth; }
        else if (text[i] == ')') {
            --depth;
            if (depth == 0) { return text.substr(list_start + 1, i - list_start - 1); }
        }
    }
    return {};
}

// Finds "key: { ... }" and returns content inside the braces.
std::string find_object_block(const std::string &text, const std::string &key) {
    const std::string needle = key + ":";
    const std::size_t begin = text.find(needle);
    if (begin == std::string::npos) { return {}; }
    const std::size_t brace_start = text.find('{', begin);
    if (brace_start == std::string::npos) { return {}; }
    int depth = 0;
    for (std::size_t i = brace_start; i < text.size(); ++i) {
        if      (text[i] == '{') { ++depth; }
        else if (text[i] == '}') {
            --depth;
            if (depth == 0) { return text.substr(brace_start + 1, i - brace_start - 1); }
        }
    }
    return {};
}

std::vector<std::string> split_objects(const std::string &block) {
    std::vector<std::string> objects;
    int depth = 0;
    std::size_t start = std::string::npos;
    for (std::size_t i = 0; i < block.size(); ++i) {
        if (block[i] == '{') {
            if (depth == 0) { start = i + 1; }
            ++depth;
        } else if (block[i] == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                objects.push_back(block.substr(start, i - start));
                start = std::string::npos;
            }
        }
    }
    return objects;
}

std::string field_value(const std::string &object, const std::string &field) {
    const std::regex pattern(field + R"__REGEX__(\s*=\s*("([^"]*)"|true|false|[^;]+)\s*;)__REGEX__");
    std::smatch match;
    if (std::regex_search(object, match, pattern)) { return strip_quotes(match[1].str()); }
    return {};
}

}  // namespace

std::vector<ManifestEntry> parse_manifest(const std::string &sw_description, const std::string &target_slot) {

    const std::string search_in = (!target_slot.empty())
        ? find_object_block(sw_description, target_slot)
        : std::string{};

    const std::string images_block =
        find_list_block(search_in.empty() ? sw_description : search_in, "images");

    if (images_block.empty()) { fail_runtime("sw-description does not contain an images list"); }

    std::vector<ManifestEntry> entries;
    for (const auto &object : split_objects(images_block)) {
        ManifestEntry entry;
        entry.filename            = field_value(object, "filename");
        entry.type                = field_value(object, "type");
        entry.compress            = field_value(object, "compress");
        entry.device              = field_value(object, "device");
        entry.path                = field_value(object, "path");
        entry.filesystem          = field_value(object, "filesystem");
        entry.sha256              = field_value(object, "sha256");
        entry.ivt                 = field_value(object, "ivt");
        entry.encrypted           = to_bool(field_value(object, "encrypted"));
        entry.preserve_attributes = to_bool(field_value(object, "preserve-attributes"));
        entry.create_destination  = to_bool(field_value(object, "create-destination"));
        entry.atomic_install      = to_bool(field_value(object, "atomic-install"));
        if (entry.filename.empty()) { continue; }
        if (entry.type.empty()) { entry.type = "raw"; }
        if (entry.type != "raw" && entry.type != "archive" && entry.type != "tar") { continue; }
        entries.push_back(std::move(entry));
    }

    if (entries.empty()) { fail_runtime("no supported raw/archive/tar entries found in sw-description"); }
    return entries;
}

ManifestEntry *find_manifest_entry(std::vector<ManifestEntry> &entries, const std::string &name) {
    for (auto &entry : entries) {
        if (entry.filename == name) { return &entry; }
    }
    return nullptr;
}

AesMaterial parse_aes_key_file(const std::string &path) {
    std::ifstream input(path);
    if (!input) { fail_runtime("cannot open AES key file"); }
    AesMaterial out;
    input >> out.key_hex >> out.iv_hex;
    if (out.key_hex.empty() || out.iv_hex.empty()) {
        fail_runtime("AES key file must contain '<key> <ivt>'");
    }
    return out;
}

}  // namespace aegis
