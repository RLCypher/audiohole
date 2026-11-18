#pragma once

#include <inspectable.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <winstring.h>

namespace audiohole::plug_board::windows_detail {

#if !defined(_M_X64) && !defined(__x86_64__)
#error The investigated private policy ABI is restricted to Windows x64.
#endif

// Private Windows interface metadata, independently expressed in C++. The IID,
// IInspectable base, nineteen preceding slots and two method signatures were
// investigated from the pinned reference recorded in EARTRUMPET_RESEARCH.md.
// No EarTrumpet implementation is imported. This is an ABI dependency, not a
// documented Microsoft SDK contract or a Windows-version compatibility promise.
//
// The preceding operations are intentionally unnamed and NEVER called. Their
// placeholder signatures preserve vtable positions only; they are not callable
// declarations of those unrelated methods. A different IID is Unsupported.
inline constexpr IID policy_factory_21h2_iid{
    0xab3d4648, 0xe242, 0x459f, {0xb0, 0x2f, 0x54, 0x1c, 0x70, 0x30, 0x63, 0x24}};

struct PolicyFactory21H2 : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Reserved01() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved02() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved03() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved04() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved05() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved06() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved07() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved08() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved09() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved10() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved11() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved12() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved13() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved14() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved15() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved16() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved17() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved18() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reserved19() = 0;

    virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint(UINT32 process_id,
                                                                       EDataFlow flow, ERole role,
                                                                       HSTRING device_id) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint(UINT32 process_id,
                                                                       EDataFlow flow, ERole role,
                                                                       HSTRING* device_id) = 0;

    // Deliberately do not expose the following global-clear operation. A local
    // reversible experiment must never reset other applications' preferences.
};

static_assert(sizeof(void*) == 8, "Only the reviewed x64 interface layout is selected.");

} // namespace audiohole::plug_board::windows_detail
