// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/local_ai/local_ai_service_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "brave/components/constants/webui_url_constants.h"
#include "brave/components/local_ai/content/background_contents_host_impl.h"
#include "brave/components/local_ai/core/background_contents_host.h"
#include "brave/components/local_ai/core/local_ai_service.h"
#include "brave/grit/brave_generated_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "url/gurl.h"

namespace local_ai {

// static
LocalAIServiceFactory* LocalAIServiceFactory::GetInstance() {
  static base::NoDestructor<LocalAIServiceFactory> instance;
  return instance.get();
}

// static
mojo::PendingRemote<mojom::LocalAIService> LocalAIServiceFactory::GetForProfile(
    Profile* profile) {
  auto* service = GetServiceForProfile(profile);
  if (!service) {
    return {};
  }
  return service->MakeRemote();
}

// static
void LocalAIServiceFactory::BindForProfile(
    Profile* profile,
    mojo::PendingReceiver<mojom::LocalAIService> receiver) {
  auto* service = GetServiceForProfile(profile);
  if (service) {
    service->Bind(std::move(receiver));
  }
}

// static
LocalAIService* LocalAIServiceFactory::GetServiceForProfile(Profile* profile) {
  return static_cast<LocalAIService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

LocalAIServiceFactory::LocalAIServiceFactory()
    : ProfileKeyedServiceFactory(
          "LocalAIService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

LocalAIServiceFactory::~LocalAIServiceFactory() = default;

std::unique_ptr<KeyedService>
LocalAIServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto factory = base::BindRepeating(
      [](content::BrowserContext* browser_context,
         LocalAIService::BackgroundContentsDestroyedCallback destroyed_callback)
          -> std::unique_ptr<BackgroundContentsHost> {
        return std::make_unique<BackgroundContentsHostImpl>(
            browser_context, GURL(kUntrustedOnDeviceModelWorkerURL),
            base::BindOnce([](content::WebContents* web_contents) {
              task_manager::WebContentsTags::CreateForToolContents(
                  web_contents, IDS_LOCAL_AI_TASK_MANAGER_TITLE);
            }),
            std::move(destroyed_callback));
      },
      context);

  return std::make_unique<LocalAIService>(std::move(factory));
}

}  // namespace local_ai
