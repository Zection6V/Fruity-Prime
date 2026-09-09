/*
 * Native counterpart of MphRead/Mods/Update/UpdateDownload.cs.
 *
 * The release URL is validated again at the write boundary. A complete body
 * is first placed beside the destination as `.part`; only then is it moved
 * into the name the installer/front-end will consume.
 */
#include "Mods/Update/update.hpp"

#include "update_http.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fruityprime::update {

DownloadResult UpdateDownload::fetch(
    std::string_view url, const std::string& path,
    std::uint64_t expected_bytes, const std::function<void(float)>& progress) {
    DownloadResult result;
    if (!UpdateCheck::is_allowed_url(url)) {
        result.error = "that download address is not GitHub's";
        return result;
    }
    if (path.empty()) {
        result.error = "download destination is empty";
        return result;
    }

    const std::filesystem::path destination(path);
    const std::filesystem::path partial = path + ".part";
    const auto cleanup_partial = [&]() noexcept {
        std::error_code error;
        std::filesystem::remove(partial, error);
    };
    cleanup_partial();

    try {
        if (destination.has_parent_path()) {
            std::error_code error;
            std::filesystem::create_directories(destination.parent_path(),
                                                error);
            if (error) {
                throw std::runtime_error(
                    "could not create download directory: "
                    + error.message());
            }
        }
        if (progress) {
            progress(0.0F);
        }
        const http::Response response = http::get(
            url, 10 * 60 * 1000,
            std::string("FruityPrime/")
                + BuildVersion::display(BuildVersion::current()));
        if (!response.error.empty()) {
            throw std::runtime_error(response.error);
        }
        if (!response.success()) {
            throw std::runtime_error("GitHub answered "
                                     + std::to_string(response.status_code));
        }
        const std::uint64_t total = response.content_length != 0
            ? response.content_length : expected_bytes;
        if (total != 0 && response.body.size() != total) {
            throw std::runtime_error("the download ended early");
        }
        if (expected_bytes != 0
            && response.body.size() != expected_bytes) {
            throw std::runtime_error("the downloaded package has the wrong size");
        }

        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("could not create partial download");
        }
        output.write(response.body.data(),
                     static_cast<std::streamsize>(response.body.size()));
        if (!output) {
            throw std::runtime_error("could not write partial download");
        }
        output.close();

        std::error_code error;
        std::filesystem::remove(destination, error);
        if (error) {
            throw std::runtime_error("could not replace old download: "
                                     + error.message());
        }
        std::filesystem::rename(partial, destination, error);
        if (error) {
            throw std::runtime_error("could not publish download: "
                                     + error.message());
        }
        if (progress) {
            progress(1.0F);
        }
        result.ok = true;
    } catch (const std::exception& exception) {
        result.error = exception.what();
        cleanup_partial();
    }
    return result;
}

} // namespace fruityprime::update
