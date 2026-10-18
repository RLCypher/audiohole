#pragma once

#include <objbase.h>

#if !defined(_M_X64) && !defined(__x86_64__)
#error AudioHole currently targets Windows x64 only.
#endif

namespace audiohole::windows {

enum class ApartmentModel { multi_threaded, single_threaded };

// A thread-bound COM initialization scope, not an owner of COM interfaces.
// Construct and destroy on the same thread. It must outlive that thread's COM
// objects. S_FALSE also owns an initialization count; failed calls do not.
class ComApartment final {
public:
    explicit ComApartment(
        ApartmentModel model = ApartmentModel::multi_threaded) noexcept;
    ~ComApartment() noexcept;

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;
    ComApartment(ComApartment&&) = delete;
    ComApartment& operator=(ComApartment&&) = delete;

    [[nodiscard]] HRESULT result() const noexcept { return result_; }
    [[nodiscard]] bool initialized() const noexcept { return SUCCEEDED(result_); }

private:
    HRESULT result_;
};

}  // namespace audiohole::windows
