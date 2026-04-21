#pragma once

#include <string>
#include <vector>

namespace aegis {

struct PackOptions {
    std::string sw_description;
    std::string sw_description_sig;
    std::string output_path;
    std::vector<std::string> payloads;
};

class Packer {
public:
    explicit Packer(const PackOptions &options);
    int pack();

private:
    const PackOptions &options_;
};

}  // namespace aegis
