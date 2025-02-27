// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/ios/browser/language_detection_controller.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/time/time.h"
#include "components/translate/core/common/translate_pref_names.h"
#include "components/translate/core/language_detection/language_detection_util.h"
#import "components/translate/ios/browser/js_language_detection_manager.h"
#include "ios/web/public/string_util.h"
#import "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_state/web_state.h"

namespace translate {

namespace {
// Name for the UMA metric used to track text extraction time.
const char kTranslateCaptureText[] = "Translate.CaptureText";
// Prefix for the language detection javascript commands. Must be kept in sync
// with language_detection.js.
const char kCommandPrefix[] = "languageDetection";
}

LanguageDetectionController::LanguageDetectionController(
    web::WebState* web_state,
    JsLanguageDetectionManager* manager,
    PrefService* prefs)
    : web::WebStateObserver(web_state),
      js_manager_([manager retain]),
      weak_method_factory_(this) {
  DCHECK(web::WebStateObserver::web_state());
  DCHECK(js_manager_);
  translate_enabled_.Init(prefs::kEnableTranslate, prefs);
  web_state->AddScriptCommandCallback(
      base::Bind(&LanguageDetectionController::OnTextCaptured,
                 base::Unretained(this)),
      kCommandPrefix);
}

LanguageDetectionController::~LanguageDetectionController() {
}

scoped_ptr<LanguageDetectionController::CallbackList::Subscription>
LanguageDetectionController::RegisterLanguageDetectionCallback(
    const Callback& callback) {
  return language_detection_callbacks_.Add(callback);
}

void LanguageDetectionController::StartLanguageDetection() {
  if (!translate_enabled_.GetValue())
    return;  // Translate disabled in preferences.
  DCHECK(web_state());
  const GURL& url = web_state()->GetVisibleURL();
  if (!web::UrlHasWebScheme(url) || !web_state()->ContentIsHTML())
    return;
  [js_manager_ inject];
  [js_manager_ startLanguageDetection];
}

bool LanguageDetectionController::OnTextCaptured(
    const base::DictionaryValue& command,
    const GURL& url,
    bool interacting) {
  std::string textCapturedCommand;
  if (!command.GetString("command", &textCapturedCommand) ||
      textCapturedCommand != "languageDetection.textCaptured" ||
      !command.HasKey("translationAllowed")) {
    NOTREACHED();
    return false;
  }
  bool translation_allowed = false;
  command.GetBoolean("translationAllowed", &translation_allowed);
  if (!translation_allowed) {
    // Translation not allowed by the page. Done processing.
    return true;
  }
  if (!command.HasKey("captureTextTime") || !command.HasKey("htmlLang") ||
      !command.HasKey("httpContentLanguage")) {
    NOTREACHED();
    return false;
  }

  int capture_text_time = 0;
  command.GetInteger("captureTextTime", &capture_text_time);
  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeDelta::FromMillisecondsD(capture_text_time));
  std::string html_lang;
  command.GetString("htmlLang", &html_lang);
  std::string http_content_language;
  command.GetString("httpContentLanguage", &http_content_language);
  // If there is no language defined in httpEquiv, use the HTTP header.
  if (http_content_language.empty())
    http_content_language = web_state()->GetContentLanguageHeader();

  [js_manager_ retrieveBufferedTextContent:
                   base::Bind(&LanguageDetectionController::OnTextRetrieved,
                              weak_method_factory_.GetWeakPtr(),
                              http_content_language, html_lang)];
  return true;
}

void LanguageDetectionController::OnTextRetrieved(
    const std::string& http_content_language,
    const std::string& html_lang,
    const base::string16& text_content) {
  std::string language = translate::DeterminePageLanguage(
      http_content_language, html_lang,
      web::GetStringByClippingLastWord(text_content,
                                       language_detection::kMaxIndexChars),
      nullptr /* cld_language */, nullptr /* is_cld_reliable */);
  if (language.empty())
    return;  // No language detected.

  DetectionDetails details;
  details.content_language = http_content_language;
  details.html_root_language = html_lang;
  details.adopted_language = language;
  language_detection_callbacks_.Notify(details);
}

// web::WebStateObserver implementation:

void LanguageDetectionController::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS)
    StartLanguageDetection();
}

void LanguageDetectionController::UrlHashChanged() {
  StartLanguageDetection();
}

void LanguageDetectionController::HistoryStateChanged() {
  StartLanguageDetection();
}

void LanguageDetectionController::WebStateDestroyed() {
  web_state()->RemoveScriptCommandCallback(kCommandPrefix);
}

}  // namespace translate
