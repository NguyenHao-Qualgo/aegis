#pragma once

#include <map>
#include <string>
#include <vector>

namespace aegis {

void logDebug(const std::string& msg);
void logInfo(const std::string& msg);
void logWarn(const std::string& msg);
void logError(const std::string& msg);

bool startsWith(const std::string& value, const std::string& prefix);
std::string trim(const std::string& value);
std::vector<std::string> split(const std::string& value, char delim);
std::string joinPath(const std::string& lhs, const std::string& rhs);
std::string currentTimestamp();
bool fileExists(const std::string& path);
std::string readFile(const std::string& path);
void writeFile(const std::string& path, const std::string& content);
std::string shellQuote(const std::string& value);
std::string getOptionValue(const std::vector<std::string>& args, const std::string& option);
bool hasOption(const std::vector<std::string>& args, const std::string& option);

}  // namespace aegis
