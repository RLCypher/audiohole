#include "platform/windows/update_service.h"
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bcrypt.h>
#include <cwctype>
#include <mutex>
#include <objbase.h>
#include <thread>
#include <vector>
#include <winhttp.h>

namespace audiohole::windows {
namespace {
namespace fs = std::filesystem;
struct InternetHandle {
    HINTERNET value = nullptr;
    ~InternetHandle() {
        if (value)
            WinHttpCloseHandle(value);
    }
};
void checked(bool ok, const char* operation) {
    if (!ok)
        throw std::runtime_error(std::string(operation) + " (Windows error " +
                                 std::to_string(GetLastError()) + ").");
}
class Http final : public UpdateTransport {
  public:
    void cancel() override {
        std::lock_guard lock(mutex_);
        if (request_) {
            WinHttpCloseHandle(request_);
            request_ = nullptr;
        }
    }
    void fetch(const std::string& address, std::uint64_t limit, std::stop_token stop,
               const Sink& sink) override {
        InternetHandle session{WinHttpOpen(L"AudioHole updater",
                                           WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
        checked(session.value != nullptr, "Cannot open update connection");
        checked(WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000),
                "Cannot set update timeouts");
        std::wstring url(address.begin(), address.end());
        for (unsigned redirects = 0; redirects <= 5; ++redirects) {
            if (stop.stop_requested())
                throw std::runtime_error("Update cancelled.");
            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
                static_cast<DWORD>(-1);
            parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
            checked(WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts),
                    "Invalid update URL");
            std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            std::transform(host.begin(), host.end(), host.begin(),
                           [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
            const bool api = address == updates::latest_release_url;
            const bool trusted = api ? host == L"api.github.com"
                                     : host == L"github.com" ||
                                           host == L"release-assets.githubusercontent.com" ||
                                           host == L"objects.githubusercontent.com" ||
                                           host == L"github-releases.githubusercontent.com";
            if (!trusted || parts.nScheme != INTERNET_SCHEME_HTTPS ||
                parts.nPort != INTERNET_DEFAULT_HTTPS_PORT || parts.dwUserNameLength ||
                parts.dwPasswordLength || url.size() > 8192 || url.find(L'#') != std::wstring::npos)
                throw std::runtime_error("The update URL is not a trusted GitHub HTTPS address.");
            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (parts.dwExtraInfoLength)
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            InternetHandle connection{WinHttpConnect(session.value, host.c_str(), parts.nPort, 0)};
            checked(connection.value != nullptr, "Cannot connect to GitHub");
            auto request = WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
                                              WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              WINHTTP_FLAG_SECURE);
            checked(request != nullptr, "Cannot create update request");
            {
                std::lock_guard lock(mutex_);
                if (stop.stop_requested()) {
                    WinHttpCloseHandle(request);
                    throw std::runtime_error("Update cancelled.");
                }
                request_ = request;
            }
            struct RequestEnd {
                Http& owner;
                HINTERNET handle;
                ~RequestEnd() {
                    std::lock_guard lock(owner.mutex_);
                    if (owner.request_ == handle) {
                        WinHttpCloseHandle(handle);
                        owner.request_ = nullptr;
                    }
                }
            } end{*this, request};
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            checked(
                WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)),
                "Cannot set update redirect policy");
            DWORD autologon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
            checked(WinHttpSetOption(request, WINHTTP_OPTION_AUTOLOGON_POLICY, &autologon,
                                     sizeof(autologon)),
                    "Cannot disable automatic update credentials");
            checked(WinHttpSendRequest(request,
                                       api ? L"Accept: application/vnd.github+json\r\n"
                                           : L"Accept: application/octet-stream\r\n",
                                       static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0),
                    "Cannot send update request");
            checked(WinHttpReceiveResponse(request, nullptr), "Cannot read update response");
            DWORD status = 0, size = sizeof(status);
            checked(WinHttpQueryHeaders(
                        request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX),
                    "Cannot read update status");
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                size = 0;
                WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                    nullptr, &size, WINHTTP_NO_HEADER_INDEX);
                if (!size || size > 16384)
                    throw std::runtime_error("Invalid update redirect.");
                std::wstring location(size / sizeof(wchar_t), L'\0');
                checked(WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION,
                                            WINHTTP_HEADER_NAME_BY_INDEX, location.data(), &size,
                                            WINHTTP_NO_HEADER_INDEX),
                        "Cannot read update redirect");
                location.resize(location.find(L'\0'));
                url = std::move(location);
                continue;
            }
            if (status != 200)
                throw std::runtime_error("GitHub update request returned HTTP " +
                                         std::to_string(status) + ".");
            std::array<std::byte, 64 * 1024> buffer;
            std::uint64_t total = 0;
            for (;;) {
                if (stop.stop_requested())
                    throw std::runtime_error("Update cancelled.");
                DWORD read = 0;
                checked(WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()),
                                        &read),
                        "Cannot read update data");
                if (!read)
                    return;
                if (read > limit - total)
                    throw std::runtime_error("The update download exceeds its size limit.");
                total += read;
                sink(std::span(buffer.data(), read));
            }
        }
        throw std::runtime_error("Too many update redirects.");
    }

  private:
    std::mutex mutex_;
    HINTERNET request_ = nullptr;
};
struct Sha256 {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<UCHAR> object;
    Sha256() {
        try {
            DWORD bytes = 0, size = 0;
            if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
                BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&bytes),
                                  sizeof(bytes), &size, 0) < 0)
                throw std::runtime_error("Cannot initialize update verification.");
            object.resize(bytes);
            if (BCryptCreateHash(algorithm, &hash, object.data(), bytes, nullptr, 0, 0) < 0)
                throw std::runtime_error("Cannot initialize the update hash.");
        } catch (...) {
            if (hash)
                BCryptDestroyHash(hash);
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            throw;
        }
    }
    ~Sha256() {
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    void append(std::span<const std::byte> bytes) {
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),
                           static_cast<ULONG>(bytes.size()), 0) < 0)
            throw std::runtime_error("Cannot verify update data.");
    }
    std::string finish() {
        std::array<UCHAR, 32> digest{};
        if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
            throw std::runtime_error("Cannot finish update verification.");
        constexpr char hex[] = "0123456789abcdef";
        std::string result;
        for (auto c : digest) {
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        return result;
    }
};
std::uint64_t cleanup_cache(const fs::path& root) {
    if (!fs::exists(root))
        return 0;
    if (fs::is_symlink(root) || !fs::is_directory(root))
        throw std::runtime_error("Invalid update download folder.");
    unsigned count = 0;
    std::uint64_t remaining = 0;
    for (const auto& item : fs::directory_iterator(root)) {
        if (++count > 128)
            throw std::runtime_error("Too many update download folders.");
        const auto name = item.path().filename().wstring();
        GUID parsed{};
        if (!item.is_symlink() && item.is_directory() && name.size() == 47 &&
            name.starts_with(L"download-") &&
            SUCCEEDED(CLSIDFromString(name.c_str() + 9, &parsed))) {
            // Each owned folder contains exactly one installer. Never recurse
            // into a link or delete unrelated user content.
            std::error_code ignored;
            for (const auto& file : fs::directory_iterator(item.path())) {
                if (file.is_symlink() || !file.is_regular_file() ||
                    file.path().extension() != L".exe")
                    throw std::runtime_error("Unexpected content in update download folder.");
                fs::remove(file.path(), ignored);
                if (fs::exists(file.path()))
                    remaining += file.file_size();
            }
            fs::remove(item.path(), ignored);
        }
    }
    return remaining;
}
} // namespace
struct DownloadedInstaller::Impl {
    fs::path file, directory;
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool retained = false;
    ~Impl() {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        if (!retained && !directory.empty()) {
            std::error_code ignored;
            fs::remove_all(directory, ignored);
        }
    }
};
DownloadedInstaller::DownloadedInstaller(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
DownloadedInstaller::~DownloadedInstaller() = default;
const fs::path& DownloadedInstaller::path() const {
    return impl_->file;
}
void DownloadedInstaller::retain_for_installer() {
    impl_->retained = true;
}
struct UpdateService::Impl {
    fs::path cache;
    std::unique_ptr<UpdateTransport> transport;
    std::jthread worker;
    std::atomic<bool> busy = false;
    std::mutex mutex;
    std::optional<UpdateResult> result;
    void publish(UpdateResult value) {
        std::lock_guard lock(mutex);
        result = std::move(value);
        busy = false;
    }
};
UpdateService::UpdateService(fs::path cache, std::unique_ptr<UpdateTransport> transport)
    : impl_(std::make_unique<Impl>()) {
    impl_->cache = std::move(cache);
    impl_->transport = transport ? std::move(transport) : std::make_unique<Http>();
}
UpdateService::~UpdateService() {
    cancel();
    if (impl_->worker.joinable())
        impl_->worker.join();
}
bool UpdateService::busy() const {
    return impl_->busy;
}
void UpdateService::cancel() {
    impl_->worker.request_stop();
    impl_->transport->cancel();
}
std::optional<UpdateResult> UpdateService::take_result() {
    std::lock_guard lock(impl_->mutex);
    auto result = std::move(impl_->result);
    impl_->result.reset();
    return result;
}
bool UpdateService::check() {
    if (impl_->busy.exchange(true))
        return false;
    if (impl_->worker.joinable())
        impl_->worker.join();
    impl_->worker = std::jthread([this](std::stop_token stop) {
        UpdateResult result;
        try {
            cleanup_cache(impl_->cache);
            std::string bytes;
            impl_->transport->fetch(updates::latest_release_url, 512 * 1024, stop, [&](auto chunk) {
                if (chunk.size() > 512 * 1024 - bytes.size())
                    throw std::runtime_error("Release response is too large.");
                bytes.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            });
            if (stop.stop_requested())
                throw std::runtime_error("Update cancelled.");
            result.release = updates::parse_release(bytes);
        } catch (const std::exception& e) {
            result.error = e.what();
            result.cancelled = stop.stop_requested();
        }
        impl_->publish(std::move(result));
    });
    return true;
}
bool UpdateService::download(const updates::Release& release) {
    if (!release.installer || !updates::valid(release) || impl_->busy.exchange(true))
        return false;
    if (impl_->worker.joinable())
        impl_->worker.join();
    impl_->worker = std::jthread([this, release](std::stop_token stop) {
        UpdateResult result;
        try {
            const auto remaining = cleanup_cache(impl_->cache);
            if (remaining > updates::maximum_installer_bytes - release.installer->bytes)
                throw std::runtime_error("Not enough room in the bounded update download cache.");
            fs::create_directories(impl_->cache);
            GUID id{};
            if (FAILED(CoCreateGuid(&id)))
                throw std::runtime_error("Cannot create an update download folder.");
            wchar_t guid[40]{};
            StringFromGUID2(id, guid, 40);
            auto package = std::unique_ptr<DownloadedInstaller>(
                new DownloadedInstaller(std::make_unique<DownloadedInstaller::Impl>()));
            const auto directory = impl_->cache / (std::wstring(L"download-") + guid);
            if (!fs::create_directory(directory))
                throw std::runtime_error("Update download folder already exists.");
            package->impl_->directory = directory;
            package->impl_->file =
                package->impl_->directory /
                fs::path("Install AudioHole " + updates::parse_version(release.version)->display() +
                         ".exe");
            package->impl_->handle = CreateFileW(
                package->impl_->file.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            checked(package->impl_->handle != INVALID_HANDLE_VALUE,
                    "Cannot create installer download");
            Sha256 hash;
            std::uint64_t count = 0;
            std::array<std::byte, 2> signature{};
            impl_->transport->fetch(
                release.installer->url, release.installer->bytes, stop, [&](auto chunk) {
                    if (chunk.size() > release.installer->bytes - count)
                        throw std::runtime_error("Installer exceeds the published size.");
                    for (std::size_t i = 0; i < chunk.size() && count + i < 2; ++i)
                        signature[count + i] = chunk[i];
                    DWORD written = 0;
                    checked(WriteFile(package->impl_->handle, chunk.data(),
                                      static_cast<DWORD>(chunk.size()), &written, nullptr) &&
                                written == chunk.size(),
                            "Cannot save installer download");
                    hash.append(chunk);
                    count += chunk.size();
                });
            if (count != release.installer->bytes || hash.finish() != release.installer->sha256 ||
                signature[0] != std::byte{'M'} || signature[1] != std::byte{'Z'})
                throw std::runtime_error(
                    "Installer verification failed; the download was removed.");
            if (stop.stop_requested())
                throw std::runtime_error("Update cancelled.");
            checked(FlushFileBuffers(package->impl_->handle), "Cannot finish installer download");
            CloseHandle(package->impl_->handle);
            package->impl_->handle =
                CreateFileW(package->impl_->file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            checked(package->impl_->handle != INVALID_HANDLE_VALUE,
                    "Cannot seal installer download");
            // Recheck the sealed file, preventing a close/reopen substitution.
            // A read-only share permits the loader while denying write/delete.
            Sha256 sealed_hash;
            std::array<std::byte, 64 * 1024> buffer;
            std::uint64_t sealed_bytes = 0;
            for (;;) {
                DWORD read = 0;
                checked(ReadFile(package->impl_->handle, buffer.data(),
                                 static_cast<DWORD>(buffer.size()), &read, nullptr),
                        "Cannot read sealed installer");
                if (!read)
                    break;
                if (stop.stop_requested() || read > release.installer->bytes - sealed_bytes)
                    throw std::runtime_error("Installer verification cancelled or size changed.");
                sealed_bytes += read;
                sealed_hash.append(std::span(buffer.data(), read));
            }
            if (sealed_bytes != release.installer->bytes ||
                sealed_hash.finish() != release.installer->sha256)
                throw std::runtime_error("Sealed installer verification failed.");
            result.installer = std::shared_ptr<DownloadedInstaller>(std::move(package));
        } catch (const std::exception& e) {
            result.error = e.what();
            result.cancelled = stop.stop_requested();
        }
        impl_->publish(std::move(result));
    });
    return true;
}
} // namespace audiohole::windows
