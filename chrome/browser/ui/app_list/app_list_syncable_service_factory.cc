// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

#include <set>

#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/apps/drive/drive_app_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#endif

namespace app_list {

// static
AppListSyncableService* AppListSyncableServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppListSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppListSyncableServiceFactory* AppListSyncableServiceFactory::GetInstance() {
  return base::Singleton<AppListSyncableServiceFactory>::get();
}

// static
scoped_ptr<KeyedService> AppListSyncableServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return NULL;
#endif
  VLOG(1) << "BuildInstanceFor: " << profile->GetDebugName()
          << " (" << profile << ")";
  return make_scoped_ptr(new AppListSyncableService(
      profile, extensions::ExtensionSystem::Get(profile)));
}

AppListSyncableServiceFactory::AppListSyncableServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppListSyncableService",
        BrowserContextDependencyManager::GetInstance()) {
  VLOG(1) << "AppListSyncableServiceFactory()";
  typedef std::set<BrowserContextKeyedServiceFactory*> FactorySet;
  FactorySet dependent_factories;
  dependent_factories.insert(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#if defined(OS_CHROMEOS)
  dependent_factories.insert(ArcAppListPrefsFactory::GetInstance());
#endif
  DriveAppProvider::AppendDependsOnFactories(&dependent_factories);
  for (FactorySet::iterator it = dependent_factories.begin();
       it != dependent_factories.end();
       ++it) {
    DependsOn(*it);
  }
}

AppListSyncableServiceFactory::~AppListSyncableServiceFactory() {
}

KeyedService* AppListSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return BuildInstanceFor(static_cast<Profile*>(browser_context)).release();
}

void AppListSyncableServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}

content::BrowserContext* AppListSyncableServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // This matches the logic in ExtensionSyncServiceFactory, which uses the
  // orginal browser context.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppListSyncableServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Start AppListSyncableService early so that the app list positions are
  // available before the app list is opened.
  return true;
}

bool AppListSyncableServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace app_list
