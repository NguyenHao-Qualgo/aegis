#pragma once

#include <string>
#include <string>

#include "aegis/common/error.hpp"
#include "aegis/installer/payload_streamer.hpp"

namespace aegis {

void write_all_checked(int fd,
                       const char* data,
                       std::size_t len,
                       const InstallContext& ctx,
                       const std::string& broken_pipe_message = {});

void stream_payload_to_fd(PayloadStreamer& streamer,
                          StreamReader& reader,
                          const CpioEntry& cpio_entry,
                          const ManifestEntry& entry,
                          const AesMaterial* aes,
                          const InstallContext& ctx,
                          int out_fd,
                          const std::string& broken_pipe_message = {});

template <typename Sink>
void stream_payload(PayloadStreamer& streamer,
                    StreamReader& reader,
                    const CpioEntry& cpio_entry,
                    const ManifestEntry& entry,
                    const AesMaterial* aes,
                    Sink&& sink) {
    if (entry.encrypted) {
        if (!aes) {
            fail_runtime("encrypted payload requires --aes-key");
        }
        streamer.stream_encrypted(reader, cpio_entry, *aes, entry.ivt, sink, entry.sha256);
    } else {
        streamer.stream_plain(reader, cpio_entry, sink, entry.sha256);
    }
}

}  // namespace aegis
