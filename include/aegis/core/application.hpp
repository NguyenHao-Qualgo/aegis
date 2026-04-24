#pragma once

#include <vector>
#include <string>

namespace aegis {

class Application {
public:
    int run(int argc, char** argv);

private:
    int runPack(int argc, char** argv) const;
    int runDaemon(const std::vector<std::string>& args) const;
    int runCli(const std::vector<std::string>& args) const;
};

}  // namespace aegis
