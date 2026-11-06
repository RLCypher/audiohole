#include "gui/device_list.h"
#include "gui/palette.h"
#include <windows.h>
#include <wx/dc.h>
#include <wx/settings.h>
#include <wx/tooltip.h>
namespace audiohole::gui {
namespace {
class StatusDot final : public wxDataViewCustomRenderer {
  public:
    StatusDot() : wxDataViewCustomRenderer("string", wxDATAVIEW_CELL_INERT, wxALIGN_CENTER) {}
    bool SetValue(const wxVariant& value) override {
        status_ = value.GetString();
        return true;
    }
    bool GetValue(wxVariant& value) const override {
        value = status_;
        return true;
    }
    wxSize GetSize() const override {
        return GetView() ? GetView()->FromDIP(wxSize(24, 22)) : wxSize(24, 22);
    }
#if wxUSE_ACCESSIBILITY
    wxString GetAccessibleDescription() const override {
        return status_.empty() ? wxString("Hidden devices divider") : status_;
    }
#endif
    bool Render(wxRect rect, wxDC* dc, int state) override {
        if (status_.empty())
            return true;
        if (state & wxDATAVIEW_CELL_SELECTED) {
            dc->SetPen(*wxTRANSPARENT_PEN);
            dc->SetBrush(
                wxBrush(palette::selection(GetView()->GetBackgroundColour().GetLuminance() < 0.5)));
            dc->DrawRectangle(rect);
        }
        wxColour colour = status_ == "Available"     ? wxColour(40, 170, 105)
                          : status_ == "Unavailable" ? wxColour(145, 154, 168)
                          : status_ == "Unusable"    ? wxColour(230, 70, 70)
                                                     : wxColour(225, 180, 45);
        HIGHCONTRASTW contrast{};
        contrast.cbSize = sizeof(contrast);
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
            (contrast.dwFlags & HCF_HIGHCONTRASTON)) {
            if (state & wxDATAVIEW_CELL_SELECTED) {
                dc->SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc->DrawRectangle(rect);
            }
            colour = wxSystemSettings::GetColour(state & wxDATAVIEW_CELL_SELECTED
                                                     ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                                     : wxSYS_COLOUR_WINDOWTEXT);
        }
        const auto glyph = wxString::FromUTF8("●");
        const auto size = dc->GetTextExtent(glyph);
        dc->SetTextForeground(colour);
        dc->DrawText(glyph, rect.x + (rect.width - size.x) / 2,
                     rect.y + (rect.height - size.y) / 2);
        return true;
    }

  private:
    wxString status_;
};
class DeviceName final : public wxDataViewCustomRenderer {
  public:
    DeviceName() : wxDataViewCustomRenderer("string", wxDATAVIEW_CELL_INERT) {}
    bool SetValue(const wxVariant& value) override {
        name_ = value.GetString();
        return true;
    }
    bool GetValue(wxVariant& value) const override {
        value = name_;
        return true;
    }
    wxSize GetSize() const override {
        return GetView()->FromDIP(wxSize(100, 22));
    }
#if wxUSE_ACCESSIBILITY
    wxString GetAccessibleDescription() const override {
        return name_;
    }
#endif
    bool Render(wxRect rect, wxDC* dc, int state) override {
        HIGHCONTRASTW contrast{};
        contrast.cbSize = sizeof(contrast);
        const bool system =
            SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
            (contrast.dwFlags & HCF_HIGHCONTRASTON);
        const bool selected = state & wxDATAVIEW_CELL_SELECTED;
        const bool dark = GetView()->GetBackgroundColour().GetLuminance() < 0.5;
        if (selected) {
            dc->SetPen(*wxTRANSPARENT_PEN);
            dc->SetBrush(wxBrush(system ? wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)
                                        : palette::selection(dark)));
            dc->DrawRectangle(rect);
        }
        dc->SetTextForeground(system             ? wxSystemSettings::GetColour(selected
                                                                                   ? wxSYS_COLOUR_HIGHLIGHTTEXT
                                                                                   : wxSYS_COLOUR_WINDOWTEXT)
                              : selected && dark ? *wxWHITE
                                                 : GetView()->GetForegroundColour());
        const auto text = wxControl::Ellipsize(name_, *dc, wxELLIPSIZE_END, rect.width);
        dc->DrawText(text, rect.x, rect.y + (rect.height - dc->GetCharHeight()) / 2);
        return true;
    }

  private:
    wxString name_;
};
} // namespace
DeviceList::DeviceList(wxWindow* parent)
    : wxDataViewListCtrl(parent, wxID_ANY, wxDefaultPosition, wxSize(300, 100),
                         wxDV_SINGLE | wxDV_NO_HEADER) {
    SetMinSize(wxSize(120, 40));
    AppendColumn(new wxDataViewColumn("Status", new StatusDot, 0, FromDIP(30), wxALIGN_CENTER, 0));
    auto* renderer = new DeviceName;
    AppendColumn(new wxDataViewColumn("Device name", renderer, 1, 260, wxALIGN_LEFT, 0));
    Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        const int width = GetClientSize().x;
        GetColumn(1)->SetWidth(
            std::max(1, width - FromDIP(42) - wxSystemSettings::GetMetric(wxSYS_VSCROLL_X)));
        event.Skip();
    });
    // Windows uses the generic data view's child window for row mouse events.
    auto* client = GetMainWindow();
    client->Bind(wxEVT_MOTION, [this, client](wxMouseEvent& event) {
        const auto row = HitTest(ScreenToClient(client->ClientToScreen(event.GetPosition())));
        const auto tip = row >= 0 ? GetTextValue(static_cast<unsigned>(row), 0) + ": " +
                                        GetTextValue(static_cast<unsigned>(row), 1)
                                  : wxString("Right-click a device for scans and Hide / Unhide.");
        if (!client->GetToolTip() || client->GetToolTip()->GetTip() != tip)
            client->SetToolTip(tip);
        event.Skip();
    });
}
void DeviceList::Append(const wxString& name, const wxString& status) {
    wxVector<wxVariant> row;
    row.push_back(wxVariant(status));
    row.push_back(wxVariant(name));
    AppendItem(row);
}
int DeviceList::HitTest(const wxPoint& position) const {
    wxDataViewItem item;
    wxDataViewColumn* column = nullptr;
    wxDataViewListCtrl::HitTest(position, item, column);
    return item.IsOk() ? static_cast<int>(ItemToRow(item)) : wxNOT_FOUND;
}
int DeviceList::FindString(const wxString& name) const {
    for (unsigned row = 0; row < GetItemCount(); ++row)
        if (GetTextValue(row, 1) == name)
            return static_cast<int>(row);
    return wxNOT_FOUND;
}
} // namespace audiohole::gui
