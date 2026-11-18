#include "native_runtime.hpp"
#include "taskbar_icon_cache.hpp"
#include <shobjidl.h>

#include <condition_variable>
#include <future>
#include <gtest/gtest.h>
#include <mutex>
#include <thread>

#ifndef PLUG_BOARD_ICON_FIXTURE_NAME
#define PLUG_BOARD_ICON_FIXTURE_NAME "AudioHole-PlugBoard.exe"
#endif

namespace audiohole::plug_board {
namespace {
// A real message-pumping window supplies a distinct taskbar icon while the
// executable/class retains its generic icon. The exporter runs on another STA,
// exactly as in production; no audio preference interface is involved.
class IconWindow {
  public:
    HWND window{};
    std::jthread thread;
    explicit IconWindow(bool visible = true, bool tool = false, bool class_only = false,
                        bool engine_first = false) {
        auto ready = std::make_shared<std::promise<HWND>>();
        auto result = ready->get_future();
        thread = std::jthread([ready, visible, tool, class_only, engine_first] {
            const auto instance = GetModuleHandleW(nullptr);
            const auto name = L"PlugBoardIconTest-" + std::to_wstring(GetCurrentThreadId());
            WNDCLASSEXW type{};
            type.cbSize = sizeof(type);
            type.hInstance = instance;
            type.lpszClassName = name.c_str();
            type.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
            type.hIconSm = LoadIconW(nullptr, IDI_ERROR);
            type.lpfnWndProc = [](HWND handle, UINT message, WPARAM first,
                                  LPARAM second) -> LRESULT {
                if (message == WM_NCCREATE)
                    SetWindowLongPtrW(
                        handle, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(
                            reinterpret_cast<CREATESTRUCTW*>(second)->lpCreateParams));
                if (message == WM_APP + 71) {
                    SetWindowLongPtrW(handle, GWLP_USERDATA,
                                      GetWindowLongPtrW(handle, GWLP_USERDATA) & ~2);
                    return 0;
                }
                const auto flags = GetWindowLongPtrW(handle, GWLP_USERDATA);
                if (message == WM_GETICON && (flags & 1))
                    return 0;
                if (message == WM_GETICON)
                    return reinterpret_cast<LRESULT>(LoadIconW(
                        nullptr, first == ICON_BIG || (flags & 2) ? IDI_INFORMATION : IDI_ERROR));
                if (message == WM_CLOSE) {
                    DestroyWindow(handle);
                    return 0;
                }
                if (message == WM_DESTROY) {
                    PostQuitMessage(0);
                    return 0;
                }
                return DefWindowProcW(handle, message, first, second);
            };
            if (!RegisterClassExW(&type)) {
                ready->set_value(nullptr);
                return;
            }
            const auto handle = CreateWindowExW(
                tool ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW, name.c_str(), L"Taskbar icon fixture",
                WS_OVERLAPPEDWINDOW, 20, 20, 240, 120, nullptr, nullptr, instance,
                reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>((class_only ? 1 : 0) | (engine_first ? 2 : 0))));
            if (handle && visible) {
                ShowWindow(handle, SW_SHOWNOACTIVATE);
                UpdateWindow(handle);
            }
            ready->set_value(handle);
            if (handle) {
                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
            }
            UnregisterClassW(name.c_str(), instance);
        });
        window = result.get();
    }
    ~IconWindow() {
        if (window)
            PostMessageW(window, WM_CLOSE, 0, 0);
        if (thread.joinable())
            thread.join();
    }
};
// Callback values are copied into this bounded test mailbox. The production
// worker stays asynchronous and queries real windows; waits observe its output
// rather than forcing another icon request after the window becomes ready.
struct IconMailbox {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<ApplicationImage> images;
    BackendCallbacks callbacks() {
        BackendCallbacks result;
        result.icon_ready = [this](auto, auto, ApplicationImage image) {
            std::lock_guard lock(mutex);
            images.push_back(std::move(image));
            changed.notify_all();
        };
        return result;
    }
    bool await(const std::function<bool(const ApplicationImage&)>& predicate) {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(5),
                                [&] { return !images.empty() && predicate(images.back()); });
    }
    ApplicationImage last() {
        std::lock_guard lock(mutex);
        return images.back();
    }
};
struct IconCacheDirectory {
    std::filesystem::path root = std::filesystem::temp_directory_path() / new_id("icon-cache");
    ~IconCacheDirectory() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};
bool has_red_window_pixels(const ApplicationImage& image) {
    for (std::size_t index = 0; index + 3 < image.rgba.size(); index += 4)
        if (image.rgba[index] > 180 && image.rgba[index + 1] < 100 && image.rgba[index + 3] > 100)
            return true;
    return false;
}
ApplicationObservation current_app() {
    ApplicationObservation app;
    app.observation_key = "taskbar-icon-fixture";
    std::wstring image(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, image.data(), static_cast<DWORD>(image.size()));
    image.resize(size);
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    app.identity = {IdentityKind::Win32Image, windows_detail::utf8(image)};
    app.verified_processes = {
        {GetCurrentProcessId(), windows_detail::file_time(created), app.identity.value}};
    return app;
}
TEST(PlugBoardTaskbarIcon, WindowIconWinsOverTheExecutableAndClassIcon) {
    IconWindow fixture;
    ASSERT_NE(fixture.window, nullptr);
    const auto image = windows_detail::resolve_icon_pixels(current_app(), 32);
    ASSERT_TRUE(image.result.ok) << image.result.detail;
    EXPECT_EQ(image.source, "taskbar_window_icon");
    EXPECT_EQ(image.evidence.at("window_icon_kind"), "ICON_SMALL2");
    EXPECT_TRUE(image.application_specific);
    EXPECT_EQ(image.width, 32u);
    EXPECT_EQ(image.height, 32u);
    bool red = false;
    for (std::size_t index = 0; index + 3 < image.rgba.size(); index += 4)
        red = red || (image.rgba[index] > 180 && image.rgba[index + 1] < 100 &&
                      image.rgba[index + 3] > 100);
    EXPECT_TRUE(red)
        << "Expected pixels from the custom red window icon, not the generic engine icon.";
}
TEST(PlugBoardTaskbarIcon, SmallClassIconWinsWhenTheWindowUsesClassIcons) {
    IconWindow fixture(true, false, true);
    ASSERT_NE(fixture.window, nullptr);
    const auto image = windows_detail::resolve_icon_pixels(current_app(), 32);
    ASSERT_TRUE(image.result.ok) << image.result.detail;
    EXPECT_EQ(image.source, "taskbar_window_class_icon");
    EXPECT_EQ(image.evidence.at("window_icon_kind"), "CLASS_SMALL");
    bool red = false;
    for (std::size_t index = 0; index + 3 < image.rgba.size(); index += 4)
        red = red || (image.rgba[index] > 180 && image.rgba[index + 1] < 100 &&
                      image.rgba[index + 3] > 100);
    EXPECT_TRUE(red) << "Expected the taskbar-sized class icon, not the large engine class icon.";
}

TEST(PlugBoardTaskbarIcon, AReusedOrIncorrectProcessTokenCannotSupplyTheWindowIcon) {
    IconWindow fixture;
    ASSERT_NE(fixture.window, nullptr);
    auto app = current_app();
    ++app.verified_processes[0].creation_time;
    const auto window = windows_detail::observe_window_icon(app, 32);
    EXPECT_FALSE(window.icon);
}
TEST(PlugBoardTaskbarIcon, HiddenAndToolWindowsAreNotTaskbarCandidates) {
    {
        IconWindow hidden(false);
        ASSERT_NE(hidden.window, nullptr);
        EXPECT_FALSE(windows_detail::observe_window_icon(current_app(), 32).icon);
    }
    {
        IconWindow tool(true, true);
        ASSERT_NE(tool.window, nullptr);
        EXPECT_FALSE(windows_detail::observe_window_icon(current_app(), 32).icon);
    }
}
TEST(PlugBoardTaskbarIcon, AWindowCreatedAfterTheFirstLookupReplacesTheExecutableIcon) {
    IconMailbox mailbox;
    auto metrics = std::make_shared<windows_detail::NativeCounters>();
    windows_detail::IconWorker worker(mailbox.callbacks(), metrics);
    worker.request({current_app()}, 32, 1);
    ASSERT_TRUE(mailbox.await([](const auto& image) { return image.result.ok; }));
    EXPECT_FALSE(mailbox.last().source.starts_with("taskbar_"));
    IconWindow fixture;
    ASSERT_NE(fixture.window, nullptr);
    ASSERT_TRUE(mailbox.await([](const auto& image) {
        return image.source == "taskbar_window_icon" && has_red_window_pixels(image);
    })) << "The initial successful executable lookup must not freeze the icon.";
    worker.cancel_pending();
    const auto extractions = metrics->icon_extractions.load();
    std::unique_lock lock(mailbox.mutex);
    const auto count = mailbox.images.size();
    EXPECT_FALSE(mailbox.changed.wait_for(lock, std::chrono::milliseconds(350), [&] {
        return mailbox.images.size() != count;
    })) << "Hidden views must cancel startup refresh deadlines.";
    EXPECT_EQ(metrics->icon_extractions.load(), extractions);
}
TEST(PlugBoardTaskbarIcon, ATaskbarIconSetAfterStartupReplacesTheInitialEngineWindowIcon) {
    IconWindow fixture(true, false, false, true);
    ASSERT_NE(fixture.window, nullptr);
    IconMailbox mailbox;
    windows_detail::IconWorker worker(mailbox.callbacks(),
                                      std::make_shared<windows_detail::NativeCounters>());
    worker.request({current_app()}, 32, 1);
    ASSERT_TRUE(mailbox.await([](const auto& image) { return image.result.ok; }));
    EXPECT_EQ(mailbox.last().source, "taskbar_window_icon");
    EXPECT_FALSE(has_red_window_pixels(mailbox.last()));
    SendMessageW(fixture.window, WM_APP + 71, 0, 0);
    EXPECT_TRUE(mailbox.await([](const auto& image) { return has_red_window_pixels(image); }))
        << "A window's first engine icon is not necessarily its final taskbar icon.";
}
TEST(PlugBoardTaskbarIcon,
     ClosedApplicationRetainsItsTaskbarIconAcrossWorkerRestartsAndDpiChanges) {
    IconCacheDirectory directory;
    ApplicationImage learned;
    {
        IconWindow fixture;
        IconMailbox mailbox;
        windows_detail::IconWorker worker(mailbox.callbacks(),
                                          std::make_shared<windows_detail::NativeCounters>(),
                                          directory.root);
        worker.request({current_app()}, 32, 1);
        ASSERT_TRUE(mailbox.await([](const auto& image) {
            return image.source == "taskbar_window_icon" && has_red_window_pixels(image);
        }));
        learned = mailbox.last();
    }
    ASSERT_TRUE(std::filesystem::exists(directory.root / "icons.json"));
    {
        IconMailbox mailbox;
        windows_detail::IconWorker worker(mailbox.callbacks(),
                                          std::make_shared<windows_detail::NativeCounters>(),
                                          directory.root);
        auto closed = current_app();
        closed.observation_key = "remembered:fixture";
        closed.verified_processes.clear();
        worker.request({closed}, 32, 2);
        ASSERT_TRUE(mailbox.await(
            [](const auto& image) { return image.evidence.value("retained_taskbar", false); }));
        EXPECT_EQ(mailbox.last().rgba, learned.rgba);
        EXPECT_EQ(mailbox.last().source, learned.source);
        worker.request({closed}, 64, 2);
        ASSERT_TRUE(mailbox.await([](const auto& image) { return image.width == 64; }));
        EXPECT_EQ(mailbox.last().height, 64u);
        EXPECT_TRUE(has_red_window_pixels(mailbox.last()));
        EXPECT_TRUE(mailbox.last().evidence.value("retained_taskbar", false));
        {
            IconWindow engine_class(true, false, true);
            worker.request({current_app()}, 32, 3);
            ASSERT_TRUE(mailbox.await([](const auto& image) {
                return image.width == 32 && image.evidence.value("retained_taskbar", false);
            }));
            EXPECT_EQ(mailbox.last().source, "taskbar_window_icon");
            EXPECT_EQ(mailbox.last().evidence.at("current_fallback_source"),
                      "taskbar_window_class_icon");
        }
        // A different complete image cannot borrow this remembered icon merely
        // because it has the same filename or display label.
        closed.identity.value = "C:\\Different\\plug_board_windows_tests.exe";
        closed.observation_key = "remembered:different";
        worker.request({closed}, 32, 2);
        ASSERT_TRUE(mailbox.await(
            [](const auto& image) { return !image.evidence.value("retained_taskbar", false); }));
        EXPECT_FALSE(mailbox.last().source.starts_with("taskbar_"));
    }
}
TEST(PlugBoardTaskbarIcon, CorruptLocalIconCacheIsPreservedWhileLiveWindowResolutionStillWorks) {
    IconCacheDirectory directory;
    const auto file = directory.root / "icons.json";
    ASSERT_TRUE(write_local_document(file, "{broken-icon-cache").ok);
    windows_detail::TaskbarIconCache cache(file);
    EXPECT_FALSE(cache.load_result().ok);
    EXPECT_EQ(cache.load_result().code, "icon_cache_unavailable");
    IconWindow fixture;
    const auto image = windows_detail::resolve_icon_pixels(current_app(), 32);
    ASSERT_TRUE(image.result.ok);
    cache.remember("verified-fixture", image);
    ASSERT_NE(cache.find("verified-fixture"), nullptr);
    auto engine = image;
    engine.source = "verified_executable_resource";
    engine.rgba.assign(engine.rgba.size(), 0);
    cache.remember("verified-fixture", engine);
    EXPECT_EQ(cache.find("verified-fixture")->rgba, image.rgba);
    cache.flush();
    EXPECT_EQ(read_local_document(file).bytes, "{broken-icon-cache");
}
TEST(PlugBoardTaskbarIcon, SessionMetadataChurnCannotExtendTheFiniteStartupRefreshWindow) {
    IconWindow fixture;
    IconMailbox mailbox;
    windows_detail::IconWorker worker(mailbox.callbacks(),
                                      std::make_shared<windows_detail::NativeCounters>());
    const auto started = std::chrono::steady_clock::now();
    auto app = current_app();
    worker.request({app}, 32, 1);
    ASSERT_TRUE(mailbox.await([](const auto& image) { return has_red_window_pixels(image); }));
    // Exercise the actual thirty-second production deadline, including its
    // final callback. No shortened test schedule substitutes for idle behavior.
    std::unique_lock lock(mailbox.mutex);
    mailbox.changed.wait_until(lock, started + std::chrono::seconds(31), [] { return false; });
    const auto before = mailbox.images.size();
    app.display_name = "New session display metadata, same verified process";
    worker.request({app}, 32, 1);
    ASSERT_TRUE(mailbox.changed.wait_for(lock, std::chrono::seconds(5),
                                         [&] { return mailbox.images.size() > before; }));
    const auto settled = mailbox.images.size();
    EXPECT_FALSE(mailbox.changed.wait_for(lock, std::chrono::milliseconds(1200), [&] {
        return mailbox.images.size() != settled;
    })) << "A settled process must not acquire another recurring startup refresh window.";
}
// Read a real .lnk in an isolated directory. No Shell Resolve or process launch
// is allowed, and an ambiguous same-image icon must not be picked arbitrarily.
TEST(PlugBoardTaskbarIcon, VerifiedShortcutOverridesAnEngineIconAndAmbiguityFallsBack) {
    windows_detail::Apartment apartment(true);
    ASSERT_TRUE(SUCCEEDED(apartment.result()));
    const auto app = current_app();
    const auto root = std::filesystem::temp_directory_path() / new_id("shortcut-icons");
    std::filesystem::create_directories(root);
    struct Cleanup {
        std::filesystem::path root;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    } cleanup{root};
    auto save = [&](const std::filesystem::path& path, const std::string& target,
                    const std::wstring& resource) {
        windows_detail::ComPointer<IShellLinkW> link;
        ASSERT_TRUE(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                               IID_IShellLinkW, link.put_void())));
        ASSERT_TRUE(SUCCEEDED(link->SetPath(windows_detail::wide(target).c_str())));
        ASSERT_TRUE(SUCCEEDED(link->SetIconLocation(resource.c_str(), 0)));
        windows_detail::ComPointer<IPersistFile> file;
        ASSERT_TRUE(SUCCEEDED(link->QueryInterface(IID_IPersistFile, file.put_void())));
        ASSERT_TRUE(SUCCEEDED(file->Save(path.c_str(), TRUE)));
    };
    std::wstring executable(32768, L'\0');
    const auto size =
        GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    executable.resize(size);
    const auto custom =
        std::filesystem::path(executable).parent_path() / PLUG_BOARD_ICON_FIXTURE_NAME;
    ASSERT_TRUE(std::filesystem::is_regular_file(custom))
        << "Build the embedding host or standalone icon fixture before this test.";
    save(root / L"Game.lnk", app.identity.value, custom.wstring());
    IconWindow fixture;
    const auto window = windows_detail::observe_window_icon(app, 32);
    const auto index = windows_detail::read_shortcut_icons({root});
    ASSERT_EQ(index.size(), 1u);
    const auto shortcut = windows_detail::observe_shortcut_icon(app, window, index);
    ASSERT_EQ(shortcut.match, "verified_full_image");
    const auto image = windows_detail::resolve_icon_pixels(app, 32, &window, &shortcut);
    ASSERT_TRUE(image.result.ok) << image.result.detail;
    EXPECT_EQ(image.source, "taskbar_shortcut_icon");
    EXPECT_EQ(image.evidence.at("shortcut_match"), "verified_full_image");
    auto different = app;
    different.identity.value = "C:\\Unrelated\\Different.exe";
    different.verified_processes.clear();
    EXPECT_TRUE(windows_detail::observe_shortcut_icon(different, {}, index).resource.empty());
    save(root / L"Other.lnk", app.identity.value, executable);
    const auto ambiguous = windows_detail::observe_shortcut_icon(
        app, window, windows_detail::read_shortcut_icons({root}));
    EXPECT_EQ(ambiguous.match, "ambiguous_custom_icons");
    EXPECT_TRUE(ambiguous.resource.empty());
    EXPECT_EQ(windows_detail::resolve_icon_pixels(app, 32, &window, &ambiguous).source,
              "taskbar_window_icon");
}
TEST(PlugBoardTaskbarIcon, ShortcutAppIdRequiresExactEvidenceAndPinnedPriorityWins) {
    const auto app = current_app();
    windows_detail::WindowIconSnapshot window;
    window.app_user_model_id = "Verified.Game.Id";
    const std::vector<windows_detail::ShortcutIconEntry> entries{
        {"pinned.lnk", {}, "Verified.Game.Id", "custom.ico,0", 0},
        {"start-menu.lnk", {}, "Verified.Game.Id", "another.ico,0", 1}};
    EXPECT_EQ(windows_detail::observe_shortcut_icon(app, window, entries).path, "pinned.lnk");
    window.app_user_model_id = "Different.Game.Id";
    EXPECT_TRUE(windows_detail::observe_shortcut_icon(app, window, entries).resource.empty());
}

TEST(PlugBoardInternalRouting, NativeLivePolicyRejectsTheHostingProcessBeforeAnyPolicyCall) {
    const auto app = current_app();
    auto adapter = make_windows_policy_adapter(app.verified_processes, PolicyScope::UserAssignment);
    const auto result = adapter->validate_process(app.verified_processes[0]);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.code, "internal_application");
    auto backend = make_windows_backend(false);
    ASSERT_TRUE(backend->initialize({}).ok);
    EXPECT_TRUE(backend->internal_application(backend->canonical_identity(app.identity)));
    EXPECT_NO_THROW(backend->shutdown());
}
} // namespace
} // namespace audiohole::plug_board
