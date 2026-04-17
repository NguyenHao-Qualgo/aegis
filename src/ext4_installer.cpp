#include "aegis/ext4_installer.hpp"

#include <filesystem>
#include <stdexcept>

#include "aegis/util.hpp"

namespace aegis {

Ext4Installer::Ext4Installer(CommandRunner runner) : runner_(std::move(runner)) {}

void Ext4Installer::installPayload(const std::string& payloadTarGz, const SlotConfig& slot, const std::string& workDir) const {
    if (slot.type != SlotType::Ext4) {
        throw std::runtime_error("Only ext4 is supported");
    }
    const auto mountPoint = joinPath(workDir, "mnt");
    std::filesystem::create_directories(mountPoint);
    runner_.runOrThrow("mount " + shellQuote(slot.device) + " " + shellQuote(mountPoint));
    try {
        runner_.runOrThrow("find " + shellQuote(mountPoint) + " -mindepth 1 -maxdepth 1 -exec rm -rf {} +");
        runner_.runOrThrow("tar -C " + shellQuote(mountPoint) + " -xzf " + shellQuote(payloadTarGz));
        runner_.runOrThrow("sync");
        runner_.runOrThrow("umount " + shellQuote(mountPoint));
    } catch (...) {
        try {
            runner_.run("umount " + shellQuote(mountPoint) + " 2>/dev/null");
        } catch (...) {
        }
        throw;
    }
}

}  // namespace aegis
