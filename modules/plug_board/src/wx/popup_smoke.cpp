#include "popup_smoke.hpp"
#include "dialogs.hpp"
#include "native_theme.hpp"
#include <fstream>
#include <stdexcept>
#include <windows.h>
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/textdlg.h>

namespace audiohole::plug_board::desktop {
namespace {
void check(bool valid, const std::string& message, std::vector<std::string>& assertions) {
    if (!valid)
        throw std::runtime_error(message);
    assertions.push_back(message);
}
wxDialog* active_dialog(wxDialog* exclude = nullptr) {
    wxDialog* result = nullptr;
    for (auto* window : wxTopLevelWindows) {
        auto* dialog = dynamic_cast<wxDialog*>(window);
        if (dialog && dialog != exclude && dialog->IsShown() && dialog->IsModal())
            result = dialog;
    }
    return result;
}
wxWindow* find_child(wxWindow& parent, const std::function<bool(wxWindow&)>& predicate) {
    for (auto* child : parent.GetChildren()) {
        if (predicate(*child))
            return child;
        if (auto* nested = find_child(*child, predicate))
            return nested;
    }
    return nullptr;
}
void click_id(wxDialog& dialog, int id) {
    auto* button = dialog.FindWindow(id);
    if (!button)
        throw std::runtime_error("Expected dialog button is missing");
    wxCommandEvent event(wxEVT_BUTTON, button->GetId());
    event.SetEventObject(button);
    button->GetEventHandler()->ProcessEvent(event);
}
void capture(wxWindow& window, const std::filesystem::path& file) {
    window.Update();
    const auto rectangle = window.GetScreenRect();
    wxBitmap bitmap(rectangle.width, rectangle.height);
    wxMemoryDC destination(bitmap);
    wxScreenDC source;
    destination.Blit(0, 0, rectangle.width, rectangle.height, &source, rectangle.x, rectangle.y);
    destination.SelectObject(wxNullBitmap);
    if (!bitmap.SaveFile(wxString(file.wstring()), wxBITMAP_TYPE_BMP))
        throw std::runtime_error("Dialog screenshot failed");
}
void selected_pixels(HWND window, const wxRect& rectangle, const ItemColors& colors,
                     std::vector<std::string>& assertions) {
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    const auto dc = GetDC(window);
    const auto rgb = [](const wxColour& color) {
        return RGB(color.Red(), color.Green(), color.Blue());
    };
    unsigned backgrounds = 0, foregrounds = 0;
    for (int y = rectangle.y + 2; y < rectangle.GetBottom() - 1; ++y)
        for (int x = rectangle.x + 4; x < rectangle.GetRight() - 4; ++x) {
            const auto pixel = GetPixel(dc, x, y);
            backgrounds += pixel == rgb(colors.background);
            foregrounds += pixel == rgb(colors.foreground);
        }
    ReleaseDC(window, dc);
    check(backgrounds > 20 && foregrounds >= 2,
          "Selected item paints a visible matched foreground and background", assertions);
    check(contrast_ratio(colors.background, colors.foreground) >= 4.45,
          "Selected text has readable contrast in the current theme", assertions);
}
void dialog_surface(wxDialog& dialog, const Appearance& appearance,
                    std::vector<std::string>& assertions) {
    dialog.Update();
    check(dialog.GetBackgroundColour() == appearance.surface,
          "Modal dialog body follows the chosen app theme", assertions);
    const auto dc = GetDC(static_cast<HWND>(dialog.GetHandle()));
    const auto color = GetPixel(dc, 3, 3);
    ReleaseDC(static_cast<HWND>(dialog.GetHandle()), dc);
    check(color ==
              RGB(appearance.surface.Red(), appearance.surface.Green(), appearance.surface.Blue()),
          "Visible modal body pixels match the app palette", assertions);
}
// Every scheduled inspection closes the modal even on assertion failure, then
// rethrows on the smoke driver's normal stack. No exception escapes wx dispatch.
void schedule(wxWindow& owner, std::exception_ptr& failure,
              const std::function<void(wxDialog&)>& action, int close_id,
              wxDialog* exclude = nullptr) {
    owner.CallAfter([&failure, action, close_id, exclude] {
        auto* dialog = active_dialog(exclude);
        try {
            if (!dialog)
                throw std::runtime_error("Smoke did not reach an actual visible modal dialog");
            action(*dialog);
            click_id(*dialog, close_id);
        } catch (...) {
            failure = std::current_exception();
            if (dialog && dialog->IsModal())
                dialog->EndModal(wxID_CANCEL);
        }
    });
}
} // namespace
void verify_confirmation_cancel(wxWindow& owner, const Appearance& appearance,
                                const std::function<void()>& open,
                                std::vector<std::string>& assertions) {
    std::exception_ptr failure;
    schedule(
        owner, failure,
        [&](wxDialog& dialog) {
            dialog_surface(dialog, appearance, assertions);
            check(dialog.GetDefaultItem() && dialog.GetDefaultItem()->GetId() == wxID_NO,
                  "The production socket-delete popup defaults to No", assertions);
        },
        wxID_NO);
    open();
    if (failure)
        std::rethrow_exception(failure);
}
void verify_popup_appearance(wxWindow& owner, const Appearance& appearance,
                             const std::filesystem::path& output,
                             std::vector<std::string>& assertions) {
    std::filesystem::create_directories(output);
    std::exception_ptr failure;
    auto* sockets = dynamic_cast<ListBox*>(
        find_child(owner, [](wxWindow& window) { return window.GetName() == "Sockets"; }));
    check(sockets && sockets->GetCount() >= 1,
          "Theme inspection reached the production Sockets list", assertions);
    sockets->SetSelection(0);
    sockets->SetFocus();
    wxRect socket_row;
    sockets->GetItemRect(0, socket_row);
    selected_pixels(static_cast<HWND>(sockets->GetHandle()), socket_row,
                    item_colors(appearance, true), assertions);
    schedule(
        owner, failure,
        [&](wxDialog& dialog) {
            dialog_surface(dialog, appearance, assertions);
            check(dialog.GetDefaultItem() && dialog.GetDefaultItem()->GetId() == wxID_NO,
                  "Destructive confirmation keeps No as its default", assertions);
            capture(dialog, output / "confirmation.bmp");
        },
        wxID_NO);
    const auto answer =
        show_message(&owner, "Remove this socket? Its applications will move to System.",
                     "Delete socket", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, appearance);
    if (failure)
        std::rethrow_exception(failure);
    check(answer == wxID_NO, "Themed confirmation returns the actual No button result", assertions);

    wxTextEntryDialog rename(&owner, "Socket name", "Rename socket", "Selected socket name");
    schedule(
        owner, failure,
        [&](wxDialog& dialog) {
            dialog_surface(dialog, appearance, assertions);
            auto* editor = dynamic_cast<wxTextCtrl*>(find_child(
                dialog, [](wxWindow& window) { return dynamic_cast<wxTextCtrl*>(&window); }));
            check(editor && editor->GetBackgroundColour() == appearance.input,
                  "Rename popup includes a themed native text editor", assertions);
            editor->SelectAll();
            capture(dialog, output / "rename.bmp");
        },
        wxID_CANCEL);
    show_themed_dialog(rename, appearance);
    if (failure)
        std::rethrow_exception(failure);

    schedule(
        owner, failure,
        [&](wxDialog& dialog) {
            dialog_surface(dialog, appearance, assertions);
            auto* list =
                dynamic_cast<ListBox*>(dialog.FindWindowByName("Available choices", &dialog));
            check(list && list->GetCount() == 2, "Choice popup exposes its actual accessible list",
                  assertions);
            list->SetSelection(1);
            list->SetFocus();
            wxRect rectangle;
            check(list->GetItemRect(1, rectangle), "Selected choice has a native item rectangle",
                  assertions);
            selected_pixels(static_cast<HWND>(list->GetHandle()), rectangle,
                            item_colors(appearance, true), assertions);
            capture(dialog, output / "choice.bmp");
        },
        wxID_OK);
    const auto selection = choose_item(&owner, "Choose a socket", {"Game", "Music"}, appearance);
    if (failure)
        std::rethrow_exception(failure);
    check(selection == 1, "Themed choice returns the selected stable index", assertions);

    const auto files = output / "files";
    std::filesystem::create_directories(files);
    std::ofstream(files / "sample.json") << "{}\n";
    std::ofstream(files / "other.txt") << "Filter test.\n";
    schedule(
        owner, failure,
        [&](wxDialog& dialog) {
            dialog_surface(dialog, appearance, assertions);
            auto* list = dynamic_cast<wxListCtrl*>(find_child(
                dialog, [](wxWindow& window) { return dynamic_cast<wxListCtrl*>(&window); }));
            check(list && list->GetItemCount() > 0, "File picker exposes actual directory entries",
                  assertions);
            const auto row = list->FindItem(-1, "sample.json");
            check(row >= 0, "File picker found the isolated test file", assertions);
            list->SetItemState(row, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                               wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            list->SetFocus();
            wxRect rectangle;
            check(list->GetItemRect(row, rectangle, wxLIST_RECT_LABEL),
                  "Selected file has a native label rectangle", assertions);
            selected_pixels(static_cast<HWND>(list->GetHandle()), rectangle,
                            item_colors(appearance, true), assertions);
            auto* filter = dynamic_cast<Choice*>(find_child(
                dialog, [](wxWindow& window) { return window.GetName() == "File type"; }));
            check(filter && filter->GetCount() == 2,
                  "File-type dropdown uses the app's themed choice control", assertions);
            const auto native = static_cast<HWND>(filter->GetHandle());
            SendMessageW(native, CB_SHOWDROPDOWN, TRUE, 0);
            COMBOBOXINFO combo{};
            combo.cbSize = sizeof(combo);
            GetComboBoxInfo(native, &combo);
            RECT item{};
            check(combo.hwndList &&
                      SendMessageW(combo.hwndList, LB_GETITEMRECT, filter->GetSelection(),
                                   reinterpret_cast<LPARAM>(&item)) != LB_ERR,
                  "File-type popup exposes its actual selected item", assertions);
            selected_pixels(
                combo.hwndList,
                wxRect(item.left, item.top, item.right - item.left, item.bottom - item.top),
                item_colors(appearance, true), assertions);
            SendMessageW(native, CB_SHOWDROPDOWN, FALSE, 0);
            check(list->FindItem(-1, "other.txt") < 0,
                  "JSON file filter initially hides non-JSON files", assertions);
            filter->SetSelection(1);
            wxCommandEvent changed(wxEVT_CHOICE, filter->GetId());
            changed.SetEventObject(filter);
            changed.SetInt(1);
            filter->GetEventHandler()->ProcessEvent(changed);
            check(list->FindItem(-1, "other.txt") >= 0,
                  "Themed file-type dropdown still updates the framework's file filter",
                  assertions);
            capture(dialog, output / "file-picker.bmp");
        },
        wxID_CANCEL);
    const auto file = choose_file(&owner, "Export diagnostics", wxString(files.wstring()),
                                  "sample.json", "JSON files (*.json)|*.json|All files|*.*",
                                  wxFD_SAVE | wxFD_OVERWRITE_PROMPT, appearance);
    if (failure)
        std::rethrow_exception(failure);
    check(!file, "Cancelling the themed file picker does not select or write a file", assertions);

    // Libraries such as the generic file browser still create wxMessageDialog
    // for validation errors. Verify the scoped replacement, with unchanged IDs.
    wxDialog outer(&owner, wxID_ANY, "Validation scope");
    outer.SetSizer(new wxBoxSizer(wxVERTICAL));
    outer.SetSize(owner.FromDIP(wxSize(360, 180)));
    owner.CallAfter([&] {
        try {
            schedule(
                owner, failure,
                [&](wxDialog& dialog) {
                    dialog_surface(dialog, appearance, assertions);
                    capture(dialog, output / "nested-validation.bmp");
                },
                wxID_OK, &outer);
            wxMessageDialog nested(&outer, "The selected file cannot be opened.", "File error",
                                   wxOK | wxICON_ERROR);
            check(nested.ShowModal() == wxID_OK,
                  "Nested library message uses the themed modal scope and preserves button IDs",
                  assertions);
        } catch (...) {
            failure = std::current_exception();
        }
        outer.EndModal(wxID_CANCEL);
    });
    show_themed_dialog(outer, appearance);
    if (failure)
        std::rethrow_exception(failure);

    wxMenu menu;
    wxMenuInvokingWindowSetter invoking(menu, &owner);
    auto* item = new MenuItem(&menu, wxID_ANY, "Selected menu option");
    menu.Append(item);
    apply_menu_appearance(menu, appearance);
    wxBitmap bitmap(280, 36);
    wxMemoryDC dc(bitmap);
    item->OnDrawItem(dc, wxRect(0, 0, 280, 36), wxOwnerDrawn::wxODDrawAll,
                     wxOwnerDrawn::wxODSelected);
    dc.SelectObject(wxNullBitmap);
    check(bitmap.SaveFile(wxString((output / "selected-menu.bmp").wstring()), wxBITMAP_TYPE_BMP),
          "Selected native menu paint is captured for review", assertions);
    const auto image = bitmap.ConvertToImage();
    const auto colors = item_colors(appearance, true);
    unsigned foregrounds = 0, backgrounds = 0;
    for (int y = 2; y < image.GetHeight() - 2; ++y)
        for (int x = 32; x < image.GetWidth() - 12; ++x) {
            const wxColour pixel(image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
            foregrounds += pixel == colors.foreground;
            backgrounds += pixel == colors.background;
        }
    check(foregrounds >= 2 && backgrounds > 20,
          "Selected menu text uses the same readable pair as lists and dropdowns", assertions);
}
} // namespace audiohole::plug_board::desktop
