// Instantiate the SDK's public GUID definitions once per standalone binary.
// MinGW's uuid library does not provide every modern Core Audio interface IID.
// Using the SDK declarations avoids hand-maintaining public GUID constants.
// initguid must precede the interface headers; sorting this block changes their
// DEFINE_GUID declarations into unresolved externs on the cross toolchain.
// clang-format off
#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <shobjidl.h>
#include <wbemidl.h>
#include <wincodec.h>
#include <propkey.h>
// clang-format on
