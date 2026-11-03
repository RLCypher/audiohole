# Proposed Frame hooks

These snippets refer to actual inspected methods at revision `54314f56d08ca`.
They are intentionally kept inside the avatar workspace for primary-agent review.
Use the real application's headers when integrating; inspection copies are only
the reproducible build reference for this standalone handoff.

```cpp
// frame.h: avatar-only members; retain the existing brand logo/tray machinery.
#include "audiohole/avatar/wx_panel.hpp"
#include "presenter.hpp"
audiohole::avatar::Panel* agent_display_{};
std::unique_ptr<audiohole::avatar::integration::Presenter> avatar_presenter_;
wxTimer avatar_timer_{this};
```

```cpp
// Existing placeholder location, after the main sizers/controller are ready.
agent_display_ = new audiohole::avatar::Panel(this);
header->Add(agent_display_, 0, wxEXPAND | wxALL, FromDIP(6));
avatar_presenter_ = std::make_unique<audiohole::avatar::integration::Presenter>(
    deployed_avatar_root, *agent_display_, your_nonmodal_diagnostic_sink);
avatar_presenter_->observe(controller_->snapshot());
avatar_presenter_->select(personality_, personality_ == "vanilla" ? "logo" : "chibi");
Bind(wxEVT_TIMER, [this](wxTimerEvent&) { avatar_presenter_->poll(); },
     avatar_timer_.GetId());
avatar_timer_.Start(16);
```

The existing `layout_header()` can keep calling `fit_height(physical_height)`.
The new panel reserves width from the fixed 512×768 canvas even while loading,
without borrowing the old Norm reference or Vanilla logo.

```cpp
// Beginning of existing Frame::refresh(); no extra controller advancement.
avatar_presenter_->observe(controller_->snapshot());

// Existing select_personality(), after the personality system resolves its ID:
avatar_presenter_->observe(controller_->snapshot());
avatar_presenter_->select(personality_, personality_ == "vanilla" ? "logo" : "chibi");
layout_header();

// Existing present_message(), after Messages::show increments occurrence_.
// Observe first: a success arriving alongside a critical failure cannot mask it.
avatar_presenter_->observe(controller_->snapshot());
avatar_presenter_->on_message(event, occurrence_);

// Before destroying the frame/children (including all exit/forced-close paths):
avatar_timer_.Stop();
avatar_presenter_->shutdown();
```

Do not invoke the inspection API from production state handling. Do not write
avatar frames or inference into audio callbacks. Future backend events must be
marshalled to the GUI/controller boundary already documented in the application;
the asset worker does not consume controller snapshots or own operational truth.
