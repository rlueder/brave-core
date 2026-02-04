// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/ui/webui/local_ai_internals/local_ai_internals_ui.h"

#include <utility>

#include "brave/browser/local_ai/local_ai_service_factory.h"
#include "brave/components/constants/webui_url_constants.h"
#include "brave/components/local_ai/resources/grit/local_ai_generated.h"
#include "brave/components/local_ai/resources/grit/local_ai_generated_map.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/brave_components_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

namespace local_ai {

LocalAIInternalsPageHandler::LocalAIInternalsPageHandler(
    mojo::PendingReceiver<local_ai_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::LocalAIService> local_ai_service)
    : receiver_(this, std::move(receiver)),
      local_ai_service_(std::move(local_ai_service)) {}

LocalAIInternalsPageHandler::~LocalAIInternalsPageHandler() = default;

void LocalAIInternalsPageHandler::GenerateEmbedding(
    const std::string& text,
    GenerateEmbeddingCallback callback) {
  local_ai_service_->GenerateEmbeddings(text, std::move(callback));
}

LocalAIInternalsUI::LocalAIInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, kLocalAIInternalsHost);

  webui::SetupWebUIDataSource(source, kLocalAiGenerated,
                              IDR_LOCAL_AI_INTERNALS_HTML);
}

LocalAIInternalsUI::~LocalAIInternalsUI() = default;

void LocalAIInternalsUI::BindInterface(
    mojo::PendingReceiver<local_ai_internals::mojom::PageHandler> receiver) {
  auto* profile = Profile::FromWebUI(web_ui());
  page_handler_ = std::make_unique<LocalAIInternalsPageHandler>(
      std::move(receiver),
      LocalAIServiceFactory::GetForProfile(profile));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LocalAIInternalsUI)

///////////////////////////////////////////////////////////////////////////////

LocalAIInternalsUIConfig::LocalAIInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kLocalAIInternalsHost) {}

}  // namespace local_ai
