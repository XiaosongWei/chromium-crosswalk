// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {
const char kStartUpURL1[] = "chrome://chrome/";
const char kStartUpURL2[] = "chrome://version/";
}

// Verifies that the |kRestoreOnStartup| and |kRestoreOnStartupURLs| policies
// are honored on Chrome OS.
class RestoreOnStartupTestChromeOS : public LoginPolicyTestBase {
 public:
  RestoreOnStartupTestChromeOS();

  // LoginPolicyTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override;

  void LogInAndVerifyStartUpURLs();

 private:
  DISALLOW_COPY_AND_ASSIGN(RestoreOnStartupTestChromeOS);
};

RestoreOnStartupTestChromeOS::RestoreOnStartupTestChromeOS() {
}

void RestoreOnStartupTestChromeOS::SetUpCommandLine(
    base::CommandLine* command_line) {
  LoginPolicyTestBase::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kDisableChildAccountDetection);
}

void RestoreOnStartupTestChromeOS::GetMandatoryPoliciesValue(
    base::DictionaryValue* policy) const {
  policy->SetInteger(key::kRestoreOnStartup,
                     SessionStartupPref::kPrefValueURLs);
  scoped_ptr<base::ListValue> urls(new base::ListValue);
  urls->AppendString(kStartUpURL1);
  urls->AppendString(kStartUpURL2);
  policy->Set(key::kRestoreOnStartupURLs, std::move(urls));
}

void RestoreOnStartupTestChromeOS::LogInAndVerifyStartUpURLs() {
  LogIn(kAccountId, kAccountPassword);

  const BrowserList* const browser_list =
      BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH);
  ASSERT_EQ(1U, browser_list->size());
  const Browser* const browser = browser_list->get(0);
  ASSERT_TRUE(browser);
  const TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  ASSERT_EQ(2, tabs->count());
  EXPECT_EQ(GURL(kStartUpURL1), tabs->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GURL(kStartUpURL2), tabs->GetWebContentsAt(1)->GetURL());
}

// Verify that the policies are honored on a new user's login.
IN_PROC_BROWSER_TEST_F(RestoreOnStartupTestChromeOS, PRE_LogInAndVerify) {
  SkipToLoginScreen();
  LogInAndVerifyStartUpURLs();
}

// Verify that the policies are honored on an existing user's login.
IN_PROC_BROWSER_TEST_F(RestoreOnStartupTestChromeOS, LogInAndVerify) {
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
      content::NotificationService::AllSources()).Wait();
  LogInAndVerifyStartUpURLs();
}

}  // namespace policy
