// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/ui/views/page_action/partitioned_storage_action_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "brave/components/containers/content/browser/storage_partition_utils.h"
#include "brave/components/containers/core/common/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace {

// Scales a container icon to the requested size for the page action icon.
class ScaledContainerIconSource : public gfx::CanvasImageSource {
 public:
  ScaledContainerIconSource(const gfx::ImageSkia& source, int target_size)
      : CanvasImageSource(gfx::Size(target_size, target_size)),
        source_(source),
        target_size_(target_size) {}

  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(/*image=*/source_, 0, 0, source_.width(),
                         source_.height(), 0, 0, /*dest_w=*/target_size_,
                         /*dest_h=*/target_size_,
                         /*filter=*/true);
  }

 private:
  gfx::ImageSkia source_;
  int target_size_;
};

}  // namespace

PartitionedStorageActionView::PartitionedStorageActionView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    Browser* browser)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         /*parent_delegate=*/icon_label_bubble_delegate,
                         /*delegate=*/page_action_icon_delegate,
                         /*name_for_histograms=*/"PartitionedStorageActionView",
                         /*action_id*/ std::nullopt,
                         browser) {
  CHECK(base::FeatureList::IsEnabled(containers::features::kContainers));
  SetVisible(false);
  SetUpForInOutAnimation();
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);

  label()->SetMaximumWidthSingleLine(120);
  label()->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

PartitionedStorageActionView::~PartitionedStorageActionView() = default;

void PartitionedStorageActionView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    container_model_.reset();
    SetVisible(false);
    return;
  }

  std::string container_id =
      containers::GetContainerIdForWebContents(web_contents);
  if (container_id.empty()) {
    container_model_.reset();
    SetVisible(false);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);

  const float scale_factor =
      GetWidget()->GetCompositor()->device_scale_factor();

  std::vector<containers::ContainerModel> models =
      containers::GetContainerModelsFromPrefs(*profile->GetPrefs(),
                                              scale_factor);
  auto it =
      std::ranges::find(models, container_id, &containers::ContainerModel::id);
  if (it != models.end()) {
    container_model_ = std::move(*it);
  } else {
    // Could happen as a result of sync
    container_model_ = containers::ContainerModel::CreateForUnknown(
        container_id, scale_factor);
  }

  auto container_name = base::UTF8ToUTF16(container_model_->name());
  SetLabel(container_name, container_name);
  ResetSlideAnimation(/*show=*/!container_name.empty());
  SetVisible(true);
  UpdateIconImage();
  UpdateBackground();
}

views::BubbleDialogDelegate* PartitionedStorageActionView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& PartitionedStorageActionView::GetVectorIcon() const {
  return gfx::VectorIcon::EmptyIcon();
}

ui::ImageModel PartitionedStorageActionView::GetSizedIconImage(int size) const {
  if (!container_model_.has_value()) {
    return ui::ImageModel();
  }

  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return ui::ImageModel();
  }

  gfx::ImageSkia rasterized =
      container_model_->icon().Rasterize(color_provider);
  if (rasterized.isNull()) {
    return ui::ImageModel();
  }

  gfx::ImageSkia scaled(
      std::make_unique<ScaledContainerIconSource>(rasterized, size),
      gfx::Size(size, size));
  return ui::ImageModel::FromImageSkia(scaled);
}

SkColor PartitionedStorageActionView::GetBackgroundColor() const {
  if (container_model_.has_value()) {
    return container_model_->background_color();
  }
  return PageActionIconView::GetBackgroundColor();
}

SkColor PartitionedStorageActionView::GetForegroundColor() const {
  return SK_ColorWHITE;
}

int PartitionedStorageActionView::GetExtraInternalSpacing() const {
  // Default internal spacing is 4 (See
  // IconLabelBubbleView::GetInternalSpacing). return -1 so total is 3.
  return -1;
}

void PartitionedStorageActionView::OnExecuting(
    PageActionIconView::ExecuteSource source) {}

BEGIN_METADATA(PartitionedStorageActionView)
END_METADATA
