#pragma once

#include <string>
#include <vector>

#include "aegis/common/logging.hpp"

namespace aegis {

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


std::string detectFilesystemType(const std::string& device);
bool hasFilesystemType(const std::string& device, const std::string& expectedType);
void makeExt4Filesystem(const std::string& device, bool force);
std::string strip_quotes(const std::string& value);
bool is_comment_or_empty(const std::string& line);
bool is_section_header(const std::string& line, std::string& section_name);


}  // namespace aegis
