#include "aegis/manifest.h"
#include "aegis/utils.h"

#include <fstream>
#include <map>
#include <sstream>

namespace aegis {

const char* to_string(BundleFormat fmt) {
    switch (fmt) {
    case BundleFormat::Plain:
        return "plain";
    case BundleFormat::Verity:
        return "verity";
    case BundleFormat::Crypt:
        return "crypt";
    }
    return "unknown";
}

BundleFormat bundle_format_from_string(const std::string& s) {
    if (s == "plain")
        return BundleFormat::Plain;
    if (s == "verity")
        return BundleFormat::Verity;
    if (s == "crypt")
        return BundleFormat::Crypt;
    throw AegisError("Unknown bundle format: " + s);
}

// Reuse the simple INI parser concept
using Section = std::map<std::string, std::string>;
using IniData = std::map<std::string, Section>;

static IniData parse_ini_string(const std::string& content) {
    IniData ini;
    std::istringstream stream(content);
    std::string line, current_section;
    while (std::getline(stream, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);
        auto end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            line = line.substr(0, end + 1);
        if (line.empty() || line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }
        auto eq = line.find('=');
        if (eq != std::string::npos) {
            auto key = line.substr(0, eq);
            auto val = line.substr(eq + 1);
            auto kt = key.find_last_not_of(" \t");
            if (kt != std::string::npos)
                key = key.substr(0, kt + 1);
            auto vs = val.find_first_not_of(" \t");
            if (vs != std::string::npos)
                val = val.substr(vs);
            ini[current_section][key] = val;
        }
    }
    return ini;
}

static std::string get(const Section& s, const std::string& k, const std::string& d = {}) {
    auto it = s.find(k);
    return it != s.end() ? it->second : d;
}

Manifest parse_manifest(const std::string& path) {
    auto content = read_text_file(path);
    auto ini = parse_ini_string(content);
    Manifest m;

    // [update]
    if (auto it = ini.find("update"); it != ini.end()) {
        m.compatible = get(it->second, "compatible");
        m.version = get(it->second, "version");
        m.build = get(it->second, "build");
        m.description = get(it->second, "description");
    }

    // [bundle]
    if (auto it = ini.find("bundle"); it != ini.end()) {
        auto fmt = get(it->second, "format", "plain");
        m.bundle_format = bundle_format_from_string(fmt);
        m.verity_hash = get(it->second, "verity-hash");
        m.verity_salt = get(it->second, "verity-salt");
        auto vs = get(it->second, "verity-size");
        if (!vs.empty())
            m.bundle_verity_size = std::stoull(vs);
        m.crypt_key = get(it->second, "crypt-key");
    }

    // [hooks]
    if (auto it = ini.find("hooks"); it != ini.end()) {
        m.hook_filename = get(it->second, "filename");
    }

    // [handler]
    if (auto it = ini.find("handler"); it != ini.end()) {
        m.handler_name = get(it->second, "filename");
        m.handler_args = get(it->second, "args");
    }

    // [image.*]
    const std::string img_prefix = "image.";
    for (auto& [sec_name, sec] : ini) {
        if (sec_name.substr(0, img_prefix.size()) != img_prefix)
            continue;

        ManifestImage img;
        img.slotclass = sec_name.substr(img_prefix.size());
        img.filename = get(sec, "filename");
        img.sha256 = get(sec, "sha256");
        img.variant = get(sec, "variant");
        img.hooks = get(sec, "hooks");
        auto sz = get(sec, "size");
        if (!sz.empty())
            img.size = std::stoull(sz);
        m.images.push_back(std::move(img));
    }

    return m;
}

void write_manifest(const Manifest& manifest, const std::string& path) {
    std::ofstream f(path);
    if (!f)
        throw AegisError("Cannot write manifest: " + path);

    f << "[update]\n";
    f << "compatible=" << manifest.compatible << "\n";
    if (!manifest.version.empty())
        f << "version=" << manifest.version << "\n";
    if (!manifest.build.empty())
        f << "build=" << manifest.build << "\n";
    if (!manifest.description.empty())
        f << "description=" << manifest.description << "\n";
    f << "\n";

    if (manifest.bundle_format != BundleFormat::Plain) {
        f << "[bundle]\n";
        f << "format=" << to_string(manifest.bundle_format) << "\n";
        if (!manifest.verity_hash.empty())
            f << "verity-hash=" << manifest.verity_hash << "\n";
        if (!manifest.verity_salt.empty())
            f << "verity-salt=" << manifest.verity_salt << "\n";
        if (manifest.bundle_verity_size > 0)
            f << "verity-size=" << manifest.bundle_verity_size << "\n";
        if (!manifest.crypt_key.empty())
            f << "crypt-key=" << manifest.crypt_key << "\n";
        f << "\n";
    }

    if (!manifest.hook_filename.empty()) {
        f << "[hooks]\n";
        f << "filename=" << manifest.hook_filename << "\n";
        f << "\n";
    }

    if (!manifest.handler_name.empty()) {
        f << "[handler]\n";
        f << "filename=" << manifest.handler_name << "\n";
        if (!manifest.handler_args.empty())
            f << "args=" << manifest.handler_args << "\n";
        f << "\n";
    }

    for (auto& img : manifest.images) {
        f << "[image." << img.slotclass << "]\n";
        if (!img.filename.empty())
            f << "filename=" << img.filename << "\n";
        if (!img.sha256.empty())
            f << "sha256=" << img.sha256 << "\n";
        if (img.size > 0)
            f << "size=" << img.size << "\n";
        if (!img.variant.empty())
            f << "variant=" << img.variant << "\n";
        f << "\n";
    }
}

} // namespace aegis
