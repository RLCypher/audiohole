#include "socket_style.hpp"
#include "audiohole/plug_board/socket_styles.hpp"
#include "material_icons.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <wx/graphics.h>
#include <wx/image.h>

namespace audiohole::plug_board::desktop {
ItemColors socket_heading_colors(const SocketAppearance& style, const Appearance& appearance) {
    const auto color = std::find_if(socket_colors.begin(), socket_colors.end(),
                                    [&](const auto& item) { return item.id == style.color; });
    if (color == socket_colors.end() || appearance.high_contrast)
        return item_colors(appearance, false);
    const wxColour background((color->rgb >> 16) & 255, (color->rgb >> 8) & 255, color->rgb & 255);
    const auto foreground =
        contrast_ratio(background, *wxWHITE) > contrast_ratio(background, *wxBLACK) ? *wxWHITE
                                                                                    : *wxBLACK;
    return {background, foreground};
}
wxBitmap socket_icon(const std::string& id, const wxColour& foreground, const wxSize& size) {
    const auto icon = std::find_if(socket_icons.begin(), socket_icons.end(),
                                   [&](const auto& item) { return item.id == id; });
    if (icon == socket_icons.end())
        return wxNullBitmap;
    // The pinned SVG paths are normalized offline and drawn with wx's existing
    // native graphics renderer. This also works when the host disabled wx SVG
    // support, and preserves sharp, correctly colored icons at every DPI.
    wxImage image(size.x, size.y, true);
    image.InitAlpha();
    std::memset(image.GetAlpha(), 0, static_cast<std::size_t>(size.x) * size.y);
    {
        std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(image));
        if (!graphics)
            return wxNullBitmap;
        graphics->Scale(size.x, size.y);
        graphics->SetBrush(wxBrush(foreground));
        graphics->SetPen(*wxTRANSPARENT_PEN);
        auto path = graphics->CreatePath();
        for (const auto& command :
             material_icon_paths[static_cast<std::size_t>(icon - socket_icons.begin())]) {
            const auto& value = command.values;
            if (command.operation == 'M')
                path.MoveToPoint(value[0], value[1]);
            else if (command.operation == 'L')
                path.AddLineToPoint(value[0], value[1]);
            else if (command.operation == 'Q')
                path.AddQuadCurveToPoint(value[0], value[1], value[2], value[3]);
            else if (command.operation == 'C')
                path.AddCurveToPoint(value[0], value[1], value[2], value[3], value[4], value[5]);
            else
                path.CloseSubpath();
        }
        graphics->FillPath(path, wxWINDING_RULE);
    }
    return wxBitmap(image);
}
} // namespace audiohole::plug_board::desktop
