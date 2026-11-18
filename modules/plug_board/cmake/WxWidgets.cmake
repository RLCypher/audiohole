# Use the same pinned native GUI framework as AudioHole. The optional prebuilt
# path is for local cross-validation with an already-built matching wx 3.2.6;
# ordinary standalone Windows builds fetch and build the pinned source below.
set(PLUG_BOARD_WX_PREBUILT_ROOT "" CACHE PATH "Existing MinGW wxWidgets 3.2.6 build tree")
if(PLUG_BOARD_WX_PREBUILT_ROOT)
    if(NOT MINGW OR NOT EXISTS "${FETCHCONTENT_SOURCE_DIR_WXWIDGETS}/include/wx/version.h")
        message(FATAL_ERROR "Prebuilt wx requires MinGW and FETCHCONTENT_SOURCE_DIR_WXWIDGETS.")
    endif()
    set(wxwidgets_SOURCE_DIR "${FETCHCONTENT_SOURCE_DIR_WXWIDGETS}")
    set(plug_board_wx_library_root "${PLUG_BOARD_WX_PREBUILT_ROOT}/lib/gcc_x64_lib")
    if(NOT EXISTS "${plug_board_wx_library_root}/mswu/wx/setup.h")
        message(FATAL_ERROR "Prebuilt wx must contain the matching static Unicode release setup.")
    endif()
    add_library(plug_board_wx_base STATIC IMPORTED)
    add_library(plug_board_wx_core STATIC IMPORTED)
    set_target_properties(plug_board_wx_base PROPERTIES
        IMPORTED_LOCATION "${plug_board_wx_library_root}/libwxbase32u.a"
        INTERFACE_INCLUDE_DIRECTORIES "${plug_board_wx_library_root}/mswu;${wxwidgets_SOURCE_DIR}/include"
        INTERFACE_COMPILE_DEFINITIONS "__WXMSW__"
        INTERFACE_LINK_LIBRARIES "shlwapi;version")
    set_target_properties(plug_board_wx_core PROPERTIES
        IMPORTED_LOCATION "${plug_board_wx_library_root}/libwxmsw32u_core.a"
        INTERFACE_LINK_LIBRARIES "plug_board_wx_base;comdlg32;winspool;winmm;comctl32;ole32;oleaut32;rpcrt4;ws2_32;wininet;oleacc;uxtheme")
    add_library(wx::base ALIAS plug_board_wx_base)
    add_library(wx::core ALIAS plug_board_wx_core)
else()
    include(FetchContent)
    set(wxBUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(wxBUILD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
    set(wxBUILD_INSTALL OFF CACHE BOOL "" FORCE)
    set(wxBUILD_SAMPLES OFF CACHE STRING "" FORCE)
    set(wxBUILD_TESTS OFF CACHE STRING "" FORCE)
    set(wxBUILD_DEMOS OFF CACHE BOOL "" FORCE)
    foreach(feature LIBPNG LIBJPEG LIBTIFF ZLIB REGEX EXPAT NANOSVG)
        set(wxUSE_${feature} OFF CACHE STRING "" FORCE)
    endforeach()
    foreach(feature WEBVIEW WEBVIEW_EDGE WEBVIEW_IE MEDIACTRL STC AUI PROPGRID RIBBON RICHTEXT
        HTML XML XRC OPENGL SOCKETS INTERNET PROTOCOL FTP HTTP URL FS_INET WEBREQUEST SVG ARTPROVIDER_TANGO WXHTML_HELP)
        set(wxUSE_${feature} OFF CACHE BOOL "" FORCE)
    endforeach()
    FetchContent_Declare(wxwidgets
        URL https://github.com/wxWidgets/wxWidgets/releases/download/v3.2.6/wxWidgets-3.2.6.tar.bz2
        URL_HASH SHA256=939e5b77ddc5b6092d1d7d29491fe67010a2433cf9b9c0d841ee4d04acb9dce7
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
    FetchContent_MakeAvailable(wxwidgets)
endif()
