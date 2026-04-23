#pragma once

namespace aegis {

class StreamReader;
struct CpioEntry;
struct ManifestEntry;
struct AesMaterial;
struct InstallContext;

class IHandler {
public:
    virtual ~IHandler() = default;
    virtual void install(const InstallContext& ctx,
                         StreamReader& reader,
                         const CpioEntry& cpio_entry,
                         const ManifestEntry& entry,
                         const AesMaterial* aes) = 0;
};

}  // namespace aegis
