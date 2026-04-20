#pragma once

#include <memory>
#include <stdexcept>

#include "aegis/bootloader/boot_control.hpp"
#include "aegis/bootloader/uboot_control.hpp"
#include "aegis/bootloader/nvboot_control.hpp"
#include "aegis/config.hpp"
#include "aegis/types.hpp"

namespace aegis {

class BootControlFactory {
public:
    static std::unique_ptr<IBootControl> create(const BootloaderType& type,
                                                const CommandRunner& runner) {
        switch (type) {
        case BootloaderType::UBoot:
            return std::make_unique<UBootControl>(runner);
        case BootloaderType::Nvidia:
            return std::make_unique<NVBootControl>();
        }

        throw std::runtime_error("Unknown boot control backend");
    }
};

}  // namespace aegis