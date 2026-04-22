#pragma once

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <filesystem>
#include <string>

#include "logging.h"

std::string ReadFileContent(const std::string& filePath);
void WriteFileContent(const std::string& filePath, const std::string& content);
std::string decrypt(const std::string& encryptedText, const std::string& key);
std::string getWsdUUID();

int ValidateVerbose(AppLog::Level verboseLevel);
std::filesystem::path getLogfilePath(const std::string& filename);
void createFolderIfNotExist(const std::filesystem::path& path);

std::string getMacAddress();
std::string GenerateUUIDFromString(const std::string& input_str);
void touch(const std::filesystem::path& p);
std::string ReadCertificateCN(const std::string& certPath);