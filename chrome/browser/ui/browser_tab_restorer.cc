// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/user_metrics.h"

namespace chrome {
namespace {

const char kBrowserTabRestorerKey[] = "BrowserTabRestorer";

// BrowserTabRestorer is responsible for restoring a tab when the
// sessions::TabRestoreService finishes loading. A TabRestoreService is
// associated with a
// single Browser and deletes itself if the Browser is destroyed.
// BrowserTabRestorer is installed on the Profile (by way of user data), only
// one instance is created per profile at a time.
class BrowserTabRestorer : public sessions::TabRestoreServiceObserver,
                           public chrome::BrowserListObserver,
                           public base::SupportsUserData::Data {
 public:
  ~BrowserTabRestorer() override;

  static void CreateIfNecessary(Browser* browser);

 private:
  explicit BrowserTabRestorer(Browser* browser);

  // TabRestoreServiceObserver:
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;
  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  Browser* browser_;
  sessions::TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabRestorer);
};

BrowserTabRestorer::~BrowserTabRestorer() {
  tab_restore_service_->RemoveObserver(this);
  BrowserList::GetInstance(browser_->host_desktop_type())->RemoveObserver(this);
}

// static
void BrowserTabRestorer::CreateIfNecessary(Browser* browser) {
  DCHECK(browser);
  if (browser->profile()->GetUserData(kBrowserTabRestorerKey))
    return;  // Only allow one restore for a given profile at a time.

  // BrowserTabRestorer is deleted at the appropriate time.
  new BrowserTabRestorer(browser);
}

BrowserTabRestorer::BrowserTabRestorer(Browser* browser)
    : browser_(browser),
      tab_restore_service_(
          TabRestoreServiceFactory::GetForProfile(browser->profile())) {
  DCHECK(tab_restore_service_);
  DCHECK(!tab_restore_service_->IsLoaded());
  tab_restore_service_->AddObserver(this);
  BrowserList::GetInstance(browser->host_desktop_type())->AddObserver(this);
  browser_->profile()->SetUserData(kBrowserTabRestorerKey, this);
  tab_restore_service_->LoadTabsFromLastSession();
}

void BrowserTabRestorer::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {}

void BrowserTabRestorer::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {}

void BrowserTabRestorer::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  RestoreTab(browser_);
  // This deletes us.
  browser_->profile()->SetUserData(kBrowserTabRestorerKey, NULL);
}

void BrowserTabRestorer::OnBrowserRemoved(Browser* browser) {
  // This deletes us.
  browser_->profile()->SetUserData(kBrowserTabRestorerKey, NULL);
}

}  // namespace

void RestoreTab(Browser* browser) {
  content::RecordAction(base::UserMetricsAction("RestoreTab"));
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  if (!service)
    return;

  if (service->IsLoaded()) {
    service->RestoreMostRecentEntry(browser->live_tab_context(),
                                    browser->host_desktop_type());
    return;
  }

  BrowserTabRestorer::CreateIfNecessary(browser);
}

}  // namespace chrome
