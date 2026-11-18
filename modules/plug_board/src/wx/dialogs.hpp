#pragma once
#include "controls.hpp"
#include <wx/dialog.h>

namespace audiohole::plug_board::desktop {
// GUI-thread modal scope. It themes lazily created children and redirects nested
// wx native message boxes to a generic dialog with the same labels/defaults/IDs.
int show_themed_dialog(wxDialog&, const Appearance&);
int show_message(wxWindow*, const wxString& message, const wxString& title, long style,
                 const Appearance&);
std::optional<std::size_t> choose_item(wxWindow*, const wxString&, const std::vector<std::string>&,
                                       const Appearance&);
std::optional<wxString> choose_file(wxWindow*, const wxString& title, const wxString& directory,
                                    const wxString& filename, const wxString& wildcard, long style,
                                    const Appearance&);
} // namespace audiohole::plug_board::desktop
