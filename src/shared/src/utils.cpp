#include "utils.h"

#include <tinyxml2.h>
#include <uuid/uuid.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "env.h"

std::string ReadFileContent(const std::string& filePath) {
    namespace fs = std::filesystem;

    if (!fs::exists(filePath)) {
        LOG_E("File does not exist: {}", filePath);
        return "";
    }

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file) {
        LOG_E("Failed to open file: {}", filePath);
        return "";
    }

    std::ostringstream contentStream;
    contentStream << file.rdbuf();
    return contentStream.str();
}

void WriteFileContent(const std::string& filePath, const std::string& content) {
    namespace fs = std::filesystem;
    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    if (!file) {
        LOG_E("Failed to open file for writing {}", filePath);
        return;
    }
    file << content;
}

std::string decrypt(const std::string& encryptedText, const std::string& key) {
    // TODO decrypt information from ECC
    return "";
}

constexpr char filePath[] = "/mnt/crypt_UDA/ONVIFService/wsd_configuration.xml";
std::string getWsdUUID() {
    namespace fs = std::filesystem;
    if (!fs::exists(filePath)) {
        return "";
    }
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filePath) != tinyxml2::XML_SUCCESS) {
        return "";
    }
    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
        return "";
    tinyxml2::XMLElement* uuidEl = root->FirstChildElement("UUID");
    if (!uuidEl || !uuidEl->GetText())
        return "";
    std::string uuid = uuidEl->GetText();
    const std::string prefix = "urn:uuid:";
    if (uuid.rfind(prefix, 0) == 0) {
        uuid = uuid.substr(prefix.length());
    }
    return uuid;
}

int ValidateVerbose(AppLog::Level verboseLevel) {
    const int low_bound = AppLog::Level::off;
    const int high_bound = AppLog::Level::trace;
    return std::max(low_bound, std::min(static_cast<int>(verboseLevel), high_bound));
}

void createFolderIfNotExist(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::filesystem::create_directories(path, ec);
    }
}

std::filesystem::path getLogfilePath(const std::string& filename) {
    auto base = Env::ConfigBaseDirectory / std::string("logs");
    createFolderIfNotExist(base);

    std::filesystem::path log_file_path{base / filename};
    return log_file_path;
}

std::string getMacAddress() {
    std::string Mac_Address;
    std::ifstream file_MAC_Address("/sys/class/net/eth0/address");
    if (file_MAC_Address.is_open()) {
        std::getline(file_MAC_Address, Mac_Address);
        file_MAC_Address.close();
        Mac_Address.erase(std::remove_if(Mac_Address.begin(), Mac_Address.end(), ::isspace), Mac_Address.end());
    }
    return Mac_Address;
}

std::string GenerateUUIDFromString(const std::string& input_str) {
    uuid_t ns_uuid;
    uuid_t out_uuid;
    uuid_parse("6ba7b810-9dad-11d1-80b4-00c04fd430c8", ns_uuid);

    uuid_generate_md5(out_uuid, ns_uuid, input_str.c_str(), input_str.size());

    char uuid_str[37];
    uuid_unparse_lower(out_uuid, uuid_str);

    return std::string(uuid_str);
}

void touch(const std::filesystem::path& p) {
    try {
        if (std::filesystem::exists(p)) {
            auto now = std::filesystem::file_time_type::clock::now();
            std::filesystem::last_write_time(p, now);
        } else {
            std::ofstream outfile(p);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
    }
}

std::string ReadCertificateCN(const std::string& certPath) {
    FILE* fp = fopen(certPath.c_str(), "r");
    if (!fp) {
        LOG_E("Failed to open certificate file: {}", certPath);
        return "";
    }

    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!cert) {
        LOG_E("Failed to parse certificate: {}", certPath);
        return "";
    }

    X509_NAME* subj = X509_get_subject_name(cert);
    if (!subj) {
        X509_free(cert);
        LOG_E("Failed to get subject from certificate: {}", certPath);
        return "";
    }

    char cn[256] = {0};
    int idx = X509_NAME_get_index_by_NID(subj, NID_commonName, -1);
    if (idx < 0) {
        X509_free(cert);
        LOG_E("CN not found in certificate: {}", certPath);
        return "";
    }
    X509_NAME_ENTRY* entry = X509_NAME_get_entry(subj, idx);
    ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    unsigned char* utf8 = nullptr;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    std::string result;
    if (len >= 0 && utf8) {
        result.assign(reinterpret_cast<char*>(utf8), len);
        OPENSSL_free(utf8);
    }
    X509_free(cert);
    return result;
}