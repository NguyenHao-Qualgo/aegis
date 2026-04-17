#include "aegis/ota_service.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <thread>

#include "aegis/bundle_extractor.hpp"
#include "aegis/util.hpp"

namespace aegis {

OtaService::OtaService(OtaConfig config,
                       BootControl bootControl,
                       BundleVerifier verifier,
                       std::vector<std::unique_ptr<IUpdateHandler>> updateHandlers,
                       StateStore stateStore)
    : config_(std::move(config)),
      bootControl_(std::move(bootControl)),
      verifier_(std::move(verifier)),
      updateHandlers_(std::move(updateHandlers)),
      stateStore_(std::move(stateStore)) {
    status_ = stateStore_.load();
    status_.bootedSlot = bootControl_.getBootedSlot();
    status_.primarySlot = bootControl_.getPrimarySlot();
}

OtaStatus OtaService::getStatus() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

void OtaService::startInstall(const std::string& bundlePath) {
    {
        std::scoped_lock lock(mutex_);
        if (installInProgress_) {
            throw std::runtime_error("An install is already in progress");
        }
        installInProgress_ = true;
        status_.lastError.clear();
        stateStore_.save(status_);
    }

    std::thread([this, bundlePath]() {
        try {
            install(bundlePath);
            finishInstall();
        } catch (const std::exception& ex) {
            failInstall(ex.what());
        } catch (...) {
            failInstall("Unknown install failure");
        }
    }).detach();
}

void OtaService::setState(OtaState state, const std::string& operation, int progress, const std::string& message) {
    std::scoped_lock lock(mutex_);
    status_.state = state;
    status_.operation = operation;
    status_.progress = progress;
    status_.message = message;
    stateStore_.save(status_);
}

void OtaService::save() {
    std::scoped_lock lock(mutex_);
    stateStore_.save(status_);
}

void OtaService::finishInstall() {
    std::scoped_lock lock(mutex_);
    installInProgress_ = false;
    stateStore_.save(status_);
}

void OtaService::failInstall(const std::string& message) {
    logError("Install failed: " + message);
    std::scoped_lock lock(mutex_);
    installInProgress_ = false;
    status_.state = OtaState::Failure;
    status_.operation = "failure";
    status_.message = "Install failed";
    status_.lastError = message;
    stateStore_.save(status_);
}

std::string OtaService::extractBundle(const std::string& bundlePath) {
    const auto workDir = joinPath(config_.dataDirectory, "bundle-work");
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);
    BundleExtractor extractor;
    extractor.extract(bundlePath, workDir, verifier_.payloadSize(bundlePath));
    return workDir;
}

void OtaService::ensureBootable(const std::string& slot) const {
    if (!bootControl_.isSlotBootable(slot)) {
        throw std::runtime_error("Slot is not bootable: " + slot);
    }
}

const IUpdateHandler& OtaService::updateHandlerFor(const std::string& imageType) const {
    for (const auto& handler : updateHandlers_) {
        if (handler->supportsImageType(imageType)) {
            return *handler;
        }
    }
    throw std::runtime_error("Unsupported rootfs payload type: " + imageType);
}

void OtaService::install(const std::string& bundlePath) {
    logInfo("Install started: " + bundlePath);

    setState(OtaState::Sync, "sync", 0, "Collecting current slot state");
    {
        std::scoped_lock lock(mutex_);
        status_.bootedSlot = bootControl_.getBootedSlot();
        status_.primarySlot = bootControl_.getPrimarySlot();
        stateStore_.save(status_);
    }
    logInfo("Booted slot: " + status_.bootedSlot + "  primary: " + status_.primarySlot);

    setState(OtaState::Download, "verify", 10, "Verifying bundle signature");
    logInfo("Verifying bundle signature");
    auto preManifest = verifier_.verifyBundle(bundlePath, config_);
    if (preManifest) {
        logInfo("Bundle signature OK  compatible=" + preManifest->compatible +
                "  version=" + preManifest->version);
    } else {
        logWarn("Bundle is unsigned (no keyring configured)");
    }

    setState(OtaState::Install, "prepare", 25, "Extracting bundle");
    logInfo("Extracting bundle payload");
    const auto extracted = extractBundle(bundlePath);
    logInfo("Bundle extracted to: " + extracted);

    setState(OtaState::Install, "verify", 40, "Verifying bundle payloads");
    logInfo("Verifying payload checksums");
    const auto manifest = preManifest ? *preManifest : verifier_.loadManifest(extracted, config_);
    verifier_.verifyPayloads(manifest, extracted);
    logInfo("Payload checksums OK  images=" + std::to_string(manifest.images.size()));

    const auto* rootfsImage = manifest.findImageBySlotClass("rootfs");
    if (rootfsImage == nullptr) {
        throw std::runtime_error("Bundle does not contain a rootfs image");
    }

    const auto targetSlotName = bootControl_.getInactiveSlot();
    const auto& targetSlot = config_.slotByBootname(targetSlotName);

    {
        std::scoped_lock lock(mutex_);
        status_.targetSlot = targetSlotName;
        status_.bundleVersion = manifest.version;
        stateStore_.save(status_);
    }

    setState(OtaState::Install, "install", 60, "Installing bundle payload");
    const auto payloadPath = joinPath(extracted, rootfsImage->filename);
    logInfo("Installing " + rootfsImage->imagetype + " image to slot " +
            targetSlotName + " (" + targetSlot.device + ")");
    updateHandlerFor(rootfsImage->imagetype).install(
        payloadPath, targetSlot, joinPath(config_.dataDirectory, "installer-work"));
    logInfo("Payload installed");

    setState(OtaState::Install, "activate", 85, "Activating target slot");
    logInfo("Activating slot: " + targetSlotName);
    bootControl_.setSlotBootable(targetSlotName, true);
    bootControl_.setPrimarySlot(targetSlotName);

    {
        std::scoped_lock lock(mutex_);
        status_.primarySlot = targetSlotName;
        stateStore_.save(status_);
    }

    setState(OtaState::Reboot, "reboot", 100, "Ready to reboot");
    logInfo("Install complete — reboot required");
}

void OtaService::markGood() {
    const auto slot = bootControl_.getBootedSlot();
    bootControl_.markGood(slot);
    std::scoped_lock lock(mutex_);
    status_.message = "Marked current slot good";
    status_.bootedSlot = slot;
    status_.primarySlot = slot;
    stateStore_.save(status_);
}

void OtaService::markBad() {
    const auto slot = bootControl_.getBootedSlot();
    bootControl_.markBad(slot);
    std::scoped_lock lock(mutex_);
    status_.message = "Marked current slot bad";
    status_.bootedSlot = slot;
    status_.primarySlot = bootControl_.getPrimarySlot();
    stateStore_.save(status_);
}

void OtaService::markActive(const std::string& slot) {
    ensureBootable(slot);
    bootControl_.setPrimarySlot(slot);
    std::scoped_lock lock(mutex_);
    status_.primarySlot = slot;
    status_.message = "Set primary slot";
    stateStore_.save(status_);
}

std::string OtaService::getPrimary() const {
    return bootControl_.getPrimarySlot();
}

std::string OtaService::getBooted() const {
    return bootControl_.getBootedSlot();
}

void OtaService::resumeAfterBoot() {
    const auto booted = bootControl_.getBootedSlot();
    const auto primary = bootControl_.getPrimarySlot();
    std::scoped_lock lock(mutex_);
    status_.bootedSlot = booted;
    status_.primarySlot = primary;
    if (status_.targetSlot && booted == *status_.targetSlot) {
        status_.state = OtaState::Commit;
        status_.operation = "commit";
        status_.message = "Booted into expected slot; waiting for mark-good";
    } else if (status_.targetSlot && booted != *status_.targetSlot) {
        status_.state = OtaState::Failure;
        status_.operation = "failure";
        status_.lastError = "Booted slot does not match expected target";
    }
    stateStore_.save(status_);
}

}  // namespace aegis
