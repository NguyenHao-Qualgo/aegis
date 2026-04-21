#pragma once

namespace aegis {

class StreamReader;
struct CpioEntry;
struct ManifestEntry;
struct AesMaterial;

class IHandler {
public:
    virtual ~IHandler() = default;
    virtual void install(StreamReader &reader,
                         const CpioEntry &cpio_entry,
                         const ManifestEntry &entry,
                         const AesMaterial *aes) = 0;
};

class RawHandler final : public IHandler {
public:
    void install(StreamReader &reader,
                 const CpioEntry &cpio_entry,
                 const ManifestEntry &entry,
                 const AesMaterial *aes) override;
};

class ArchiveHandler final : public IHandler {
public:
    void install(StreamReader &reader,
                 const CpioEntry &cpio_entry,
                 const ManifestEntry &entry,
                 const AesMaterial *aes) override;
};

}  // namespace aegis
