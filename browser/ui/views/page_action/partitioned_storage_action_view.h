// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_BROWSER_UI_VIEWS_PAGE_ACTION_PARTITIONED_STORAGE_ACTION_VIEW_H_
#define BRAVE_BROWSER_UI_VIEWS_PAGE_ACTION_PARTITIONED_STORAGE_ACTION_VIEW_H_

#include <optional>

#include "brave/browser/ui/containers/container_model.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// Page action icon that shows when the current tab is in a Brave container
// (partitioned storage). Displays the container's icon, name, and background
// color from ContainerModel.
class PartitionedStorageActionView : public PageActionIconView {
  METADATA_HEADER(PartitionedStorageActionView, PageActionIconView)

 public:
  PartitionedStorageActionView(
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate,
      Browser* browser);
  PartitionedStorageActionView(const PartitionedStorageActionView&) = delete;
  PartitionedStorageActionView& operator=(const PartitionedStorageActionView&) =
      delete;
  ~PartitionedStorageActionView() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ui::ImageModel GetSizedIconImage(int size) const override;
  SkColor GetBackgroundColor() const override;

  // IconLabelBubbleView:
  SkColor GetForegroundColor() const override;
  int GetExtraInternalSpacing() const override;

 private:
  std::optional<containers::ContainerModel> container_model_;
};

#endif  // BRAVE_BROWSER_UI_VIEWS_PAGE_ACTION_PARTITIONED_STORAGE_ACTION_VIEW_H_
