#pragma once

#include "audiohole/plug_board/view.hpp"
#include <wx/wx.h>

namespace audiohole::plug_board::desktop {

// Colors and native-control behavior mirror AudioHole's existing palette and
// control conventions. This standalone launcher does not link the host model.
using Appearance = ViewAppearance;
Appearance appearance_for(const wxString& preference);
Appearance appearance_for_window(wxWindow*);
void apply_appearance(wxWindow&, const Appearance&);

// Always paint text and its selection surface as a pair. Contrast is checked
// against the actual RGB background, including host-provided palettes.
struct ItemColors {
    wxColour background, foreground;
};
double contrast_ratio(const wxColour&, const wxColour&);
ItemColors item_colors(const Appearance&, bool selected, bool enabled = true);

class ListBox final : public wxListBox {
  public:
    ListBox(wxWindow*, wxWindowID = wxID_ANY, const wxSize& = wxDefaultSize);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool MSWOnMeasure(WXMEASUREITEMSTRUCT*) override;
    Appearance appearance;
};

// Native menus keep their keyboard/mnemonic/submenu handling. Only the paired
// colors change; Windows' unrelated system selection palette cannot override it.
class MenuItem final : public wxMenuItem {
  public:
    MenuItem(wxMenu*, int, const wxString&, wxMenu* submenu = nullptr, wxItemKind = wxITEM_NORMAL);
    Appearance appearance;

  protected:
    void GetColourToUse(wxODStatus, wxColour&, wxColour&) const override;
};

class Button final : public wxButton {
  public:
    Button(wxWindow*, const wxString& label, const wxString& help, wxWindowID = wxID_ANY);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool high_contrast = false;
};

// Windows changes wxChoice values before a wx wheel handler can prevent it.
// Suppress that native message so scrolling cannot change a routing test target.
class Choice final : public wxChoice {
  public:
    Choice(wxWindow*, wxWindowID = wxID_ANY);
    bool MSWOnDraw(WXDRAWITEMSTRUCT*) override;
    bool MSWOnMeasure(WXMEASUREITEMSTRUCT*) override;
    WXLRESULT MSWWindowProc(WXUINT, WXWPARAM, WXLPARAM) override;
    bool high_contrast = false;

  protected:
    WXDWORD MSWGetStyle(long, WXDWORD*) const override;
    wxSize DoGetBestSize() const override;

  private:
    wxBrush input_brush;
    void paint_arrow();
};

} // namespace audiohole::plug_board::desktop
