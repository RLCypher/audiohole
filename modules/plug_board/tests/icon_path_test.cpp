#include "icon_path.hpp"

#include <gtest/gtest.h>

namespace audiohole::plug_board::windows_detail {
namespace {

// The uploaded native inspection exposed E_INVALIDARG for every extended DOS
// executable path passed directly to Shell. These portable cases pin the lookup
// spelling without changing canonical application identity or accessing a file.
TEST(PlugBoardIconPath, ExtendedSteamExecutableUsesOrdinaryShellSpelling) {
    const std::wstring canonical = L"\\\\?\\C:\\Program Files (x86)\\Steam\\steam.exe";
    EXPECT_EQ(shell_image_path(canonical), L"C:\\Program Files (x86)\\Steam\\steam.exe");
    EXPECT_EQ(canonical, L"\\\\?\\C:\\Program Files (x86)\\Steam\\steam.exe");
}

TEST(PlugBoardIconPath, UnicodeAndLetterCaseArePreserved) {
    EXPECT_EQ(shell_image_path(L"\\\\?\\d:\\音声\\RØDE Player\\Player.exe"),
              L"d:\\音声\\RØDE Player\\Player.exe");
}

TEST(PlugBoardIconPath, LiteralEnvironmentAndResourcePunctuationStayLiteral) {
    EXPECT_EQ(shell_image_path(L"\\\\?\\C:\\%APPDATA%\\Player,7.exe"),
              L"C:\\%APPDATA%\\Player,7.exe");
}

TEST(PlugBoardIconPath, OrdinaryPathsRemainUnchanged) {
    EXPECT_EQ(shell_image_path(L"C:\\Program Files\\Player\\Player.exe"),
              L"C:\\Program Files\\Player\\Player.exe");
}

TEST(PlugBoardIconPath, OtherNamespacesAreNeverBlindlyRewritten) {
    for (const std::wstring input :
         {L"\\\\?\\UNC\\server\\share\\Player.exe",
          L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolume3\\Player.exe",
          L"\\\\?\\Volume{example}\\Player.exe", L"\\\\.\\C:\\Player.exe", L"\\\\?\\C:Player.exe",
          L"\\\\?\\1:\\Player.exe", L""}) {
        EXPECT_EQ(shell_image_path(input), input);
    }
}

TEST(PlugBoardIconPath, NormalizationSensitiveComponentsKeepTheirExtendedSpelling) {
    for (const std::wstring input :
         {L"\\\\?\\C:\\Player.\\Player.exe", L"\\\\?\\C:\\Player \\Player.exe",
          L"\\\\?\\C:\\.\\Player.exe", L"\\\\?\\C:\\..\\Player.exe", L"\\\\?\\C:\\Player.exe.",
          L"\\\\?\\C:\\Player.exe ", L"\\\\?\\C:\\Player.exe:stream", L"\\\\?\\C:\\\\Player.exe",
          L"\\\\?\\C:\\folder/Player.exe"}) {
        EXPECT_EQ(shell_image_path(input), input);
    }
}

} // namespace
} // namespace audiohole::plug_board::windows_detail
