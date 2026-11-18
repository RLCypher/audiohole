// Instantiate DEFINE_GUID-based SDK constants for the linked binary.
// Core Audio's MIDL declarations do not become definitions through initguid
// with Microsoft's SDK; callers use __uuidof(interface/coclass) instead.
// Keep SDK values authoritative rather than copying GUID constants by hand.
// initguid must precede the interface headers; sorting this block changes their
// DEFINE_GUID declarations into unresolved externs on the cross toolchain.
// clang-format off
#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <propsys.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <shobjidl.h>
#include <wbemidl.h>
#include <wincodec.h>
#include <propkey.h>
// clang-format on
