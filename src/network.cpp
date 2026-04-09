#include "rauc/network.h"
#include "rauc/utils.h"

#include <curl/curl.h>
#include <cstdio>

namespace rauc {

struct DownloadState {
    FILE* file = nullptr;
    DownloadProgress progress_cb;
    uint64_t total = 0;
};

static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* state = static_cast<DownloadState*>(userdata);
    return fwrite(ptr, size, nmemb, state->file);
}

static int progress_callback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                              curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* state = static_cast<DownloadState*>(userdata);
    if (state->progress_cb && dltotal > 0) {
        state->progress_cb(static_cast<uint64_t>(dlnow),
                           static_cast<uint64_t>(dltotal));
    }
    return 0;
}

Result<void> download_bundle(const std::string& url,
                             const std::string& output_path,
                             uint64_t max_size,
                             DownloadProgress progress) {
    CURL* curl = curl_easy_init();
    if (!curl) return Result<void>::err("curl_easy_init failed");

    DownloadState state;
    state.file = fopen(output_path.c_str(), "wb");
    if (!state.file) {
        curl_easy_cleanup(curl);
        return Result<void>::err("Cannot open output: " + output_path);
    }
    state.progress_cb = progress;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    if (progress) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    if (max_size > 0) {
        curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                         static_cast<curl_off_t>(max_size));
    }

    CURLcode res = curl_easy_perform(curl);
    fclose(state.file);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        rm_rf(output_path);
        return Result<void>::err(std::string("Download failed: ") + curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    LOG_INFO("Downloaded %s to %s", url.c_str(), output_path.c_str());
    return Result<void>::ok();
}

Result<uint64_t> check_bundle_url(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return Result<uint64_t>::err("curl_easy_init failed");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        return Result<uint64_t>::err(std::string("URL check failed: ") +
                                     curl_easy_strerror(res));
    }

    curl_off_t content_length = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_length);
    curl_easy_cleanup(curl);

    return Result<uint64_t>::ok(static_cast<uint64_t>(content_length));
}

} // namespace rauc
