#pragma once
#include <wx/dataview.h>
namespace audiohole::gui {
// Accessible status cells render a colored Unicode dot; names stay compact and
// ellipsize to the draggable column width. No device checkbox or horizontal bar.
class DeviceList final : public wxDataViewListCtrl {
  public:
    explicit DeviceList(wxWindow* parent);
    void Append(const wxString& name, const wxString& status = {});
    void Clear() {
        DeleteAllItems();
    }
    unsigned GetCount() const {
        return GetItemCount();
    }
    int GetSelection() const {
        return GetSelectedRow();
    }
    void SetSelection(int index) {
        UnselectAll();
        if (index >= 0)
            SelectRow(static_cast<unsigned>(index));
    }
    int HitTest(const wxPoint&) const;
    int FindString(const wxString&) const;
};
} // namespace audiohole::gui
