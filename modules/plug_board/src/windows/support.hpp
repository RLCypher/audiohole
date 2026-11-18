#pragma once

#include "audiohole/plug_board/policy_experiment.hpp"

#include <objbase.h>
#include <roapi.h>
#include <windows.h>
#include <winstring.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace audiohole::plug_board::windows_detail {

// Wine exports some Windows APIs as aborting stubs. Detect it before entering
// native-only identity/policy paths; emulation must not become hardware evidence.
inline bool wine_runtime() {
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

// Minimal owned COM pointer. put() first releases the prior reference; callers
// must never hand it to APIs that borrow rather than return an owned reference.
template <typename Interface> class ComPointer {
  public:
    ComPointer() = default;
    ~ComPointer() {
        reset();
    }
    ComPointer(const ComPointer&) = delete;
    ComPointer& operator=(const ComPointer&) = delete;
    ComPointer(ComPointer&& other) noexcept : pointer_(std::exchange(other.pointer_, nullptr)) {}
    ComPointer& operator=(ComPointer&& other) noexcept {
        if (this != &other) {
            reset();
            pointer_ = std::exchange(other.pointer_, nullptr);
        }
        return *this;
    }
    Interface* get() const {
        return pointer_;
    }
    Interface* operator->() const {
        return pointer_;
    }
    Interface** put() {
        reset();
        return &pointer_;
    }
    void** put_void() {
        return reinterpret_cast<void**>(put());
    }
    explicit operator bool() const {
        return pointer_ != nullptr;
    }
    void reset() {
        if (pointer_) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

  private:
    Interface* pointer_ = nullptr;
};

// Thread-bound apartment initialization. The Shell worker uses STA; policy and
// session workers use WinRT MTA. S_FALSE still owns an initialization count.
class Apartment {
  public:
    explicit Apartment(bool shell_sta = false) : shell_sta_(shell_sta) {
        result_ = shell_sta ? CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)
                            : RoInitialize(RO_INIT_MULTITHREADED);
    }
    ~Apartment() {
        if (SUCCEEDED(result_)) {
            if (shell_sta_) {
                CoUninitialize();
            } else {
                RoUninitialize();
            }
        }
    }
    Apartment(const Apartment&) = delete;
    Apartment& operator=(const Apartment&) = delete;
    HRESULT result() const {
        return result_;
    }

  private:
    HRESULT result_ = E_UNEXPECTED;
    bool shell_sta_ = false;
};

class Handle {
  public:
    explicit Handle(HANDLE value = nullptr) : value_(value) {}
    ~Handle() {
        reset();
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, nullptr)) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.value_, nullptr));
        }
        return *this;
    }
    HANDLE get() const {
        return value_;
    }
    explicit operator bool() const {
        return value_ && value_ != INVALID_HANDLE_VALUE;
    }
    void reset(HANDLE value = nullptr) {
        if (*this) {
            CloseHandle(value_);
        }
        value_ = value;
    }

  private:
    HANDLE value_ = nullptr;
};

// API-returned session/device strings use CoTaskMemFree, not delete[].
class TaskString {
  public:
    ~TaskString() {
        CoTaskMemFree(value_);
    }
    TaskString() = default;
    TaskString(const TaskString&) = delete;
    TaskString& operator=(const TaskString&) = delete;
    wchar_t** put() {
        CoTaskMemFree(value_);
        value_ = nullptr;
        return &value_;
    }
    const wchar_t* get() const {
        return value_ ? value_ : L"";
    }

  private:
    wchar_t* value_ = nullptr;
};

class RuntimeString {
  public:
    RuntimeString() = default;
    explicit RuntimeString(const std::wstring& value) {
        const auto result =
            WindowsCreateString(value.c_str(), static_cast<UINT32>(value.size()), &value_);
        if (FAILED(result)) {
            throw std::runtime_error("Could not allocate a Windows Runtime string.");
        }
    }
    ~RuntimeString() {
        WindowsDeleteString(value_);
    }
    RuntimeString(const RuntimeString&) = delete;
    RuntimeString& operator=(const RuntimeString&) = delete;
    HSTRING get() const {
        return value_;
    }
    HSTRING* put() {
        WindowsDeleteString(value_);
        value_ = nullptr;
        return &value_;
    }

  private:
    HSTRING value_ = nullptr;
};

inline std::string utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int count =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (!count) {
        throw std::runtime_error("Windows returned invalid UTF-16 metadata.");
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), count, nullptr, nullptr);
    return result;
}

inline std::wstring wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (!count) {
        throw std::runtime_error("Input is not valid UTF-8.");
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), count);
    return result;
}

inline OperationResult native_result(HRESULT result, const std::string& action) {
    std::ostringstream code;
    code << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
         << static_cast<std::uint32_t>(result);
    return {SUCCEEDED(result), code.str(), action};
}

inline void require(HRESULT result, const std::string& action) {
    if (FAILED(result)) {
        const auto failure = native_result(result, action);
        throw std::runtime_error(action + " (" + failure.code + ")");
    }
}

inline std::uint64_t file_time(const FILETIME& value) {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}

} // namespace audiohole::plug_board::windows_detail
