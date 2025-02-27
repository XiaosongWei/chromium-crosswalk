// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/skbitmap_operations.h"
#include "url/gurl.h"

namespace {

// Returns the default icon image for extensions.
gfx::Image GetDefaultIcon() {
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_EXTENSIONS_FAVICON);
}

class GetAttentionImageSource : public gfx::ImageSkiaSource {
 public:
  explicit GetAttentionImageSource(const gfx::ImageSkia& icon)
      : icon_(icon) {}

  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    gfx::ImageSkiaRep icon_rep = icon_.GetRepresentation(scale);
    color_utils::HSL shift = {-1, 0, 0.5};
    return gfx::ImageSkiaRep(
        SkBitmapOperations::CreateHSLShiftedBitmap(icon_rep.sk_bitmap(), shift),
        icon_rep.scale());
  }

 private:
  const gfx::ImageSkia icon_;
};

struct IconRepresentationInfo {
  // Size as a string that will be used to retrieve a representation value from
  // SetIcon function arguments.
  const char* size_string;
  // Scale factor for which the represantion should be used.
  ui::ScaleFactor scale;
};

template <class T>
bool HasValue(const std::map<int, T>& map, int tab_id) {
  return map.find(tab_id) != map.end();
}

}  // namespace

extension_misc::ExtensionIcons ExtensionAction::ActionIconSize() {
  return ui::MaterialDesignController::IsModeMaterial()
             ? extension_misc::EXTENSION_ICON_BITTY
             : extension_misc::EXTENSION_ICON_ACTION;
}

const int ExtensionAction::kDefaultTabId = -1;

ExtensionAction::ExtensionAction(const extensions::Extension& extension,
                                 extensions::ActionInfo::Type action_type,
                                 const extensions::ActionInfo& manifest_data)
    : extension_id_(extension.id()),
      extension_name_(extension.name()),
      action_type_(action_type) {
  // Page/script actions are hidden/disabled by default, and browser actions are
  // visible/enabled by default.
  SetIsVisible(kDefaultTabId,
               action_type == extensions::ActionInfo::TYPE_BROWSER);
  Populate(extension, manifest_data);
}

ExtensionAction::~ExtensionAction() {
}

void ExtensionAction::SetPopupUrl(int tab_id, const GURL& url) {
  // We store |url| even if it is empty, rather than removing a URL from the
  // map.  If an extension has a default popup, and removes it for a tab via
  // the API, we must remember that there is no popup for that specific tab.
  // If we removed the tab's URL, GetPopupURL would incorrectly return the
  // default URL.
  SetValue(&popup_url_, tab_id, url);
}

bool ExtensionAction::HasPopup(int tab_id) const {
  return !GetPopupUrl(tab_id).is_empty();
}

GURL ExtensionAction::GetPopupUrl(int tab_id) const {
  return GetValue(&popup_url_, tab_id);
}

void ExtensionAction::SetIcon(int tab_id, const gfx::Image& image) {
  SetValue(&icon_, tab_id, image);
}

bool ExtensionAction::ParseIconFromCanvasDictionary(
    const base::DictionaryValue& dict,
    gfx::ImageSkia* icon) {
  for (base::DictionaryValue::Iterator iter(dict); !iter.IsAtEnd();
       iter.Advance()) {
    const base::BinaryValue* image_data;
    std::string binary_string64;
    IPC::Message pickle;
    if (iter.value().GetAsBinary(&image_data)) {
      pickle = IPC::Message(image_data->GetBuffer(), image_data->GetSize());
    } else if (iter.value().GetAsString(&binary_string64)) {
      std::string binary_string;
      if (!base::Base64Decode(binary_string64, &binary_string))
        return false;
      pickle = IPC::Message(binary_string.c_str(), binary_string.length());
    } else {
      continue;
    }
    base::PickleIterator pickle_iter(pickle);
    SkBitmap bitmap;
    if (!IPC::ReadParam(&pickle, &pickle_iter, &bitmap))
      return false;
    CHECK(!bitmap.isNull());

    // Chrome helpfully scales the provided icon(s), but let's not go overboard.
    const int kActionIconMaxSize = 10 * extension_misc::EXTENSION_ICON_ACTION;
    if (bitmap.drawsNothing() || bitmap.width() > kActionIconMaxSize)
      continue;

    float scale =
        static_cast<float>(bitmap.width()) / ExtensionAction::ActionIconSize();
    icon->AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return true;
}

gfx::Image ExtensionAction::GetExplicitlySetIcon(int tab_id) const {
  return GetValue(&icon_, tab_id);
}

bool ExtensionAction::SetIsVisible(int tab_id, bool new_visibility) {
  const bool old_visibility = GetValue(&is_visible_, tab_id);

  if (old_visibility == new_visibility)
    return false;

  SetValue(&is_visible_, tab_id, new_visibility);

  return true;
}

void ExtensionAction::DeclarativeShow(int tab_id) {
  DCHECK_NE(tab_id, kDefaultTabId);
  ++declarative_show_count_[tab_id];  // Use default initialization to 0.
}

void ExtensionAction::UndoDeclarativeShow(int tab_id) {
  int& show_count = declarative_show_count_[tab_id];
  DCHECK_GT(show_count, 0);
  if (--show_count == 0)
    declarative_show_count_.erase(tab_id);
}

void ExtensionAction::DeclarativeSetIcon(int tab_id,
                                         int priority,
                                         const gfx::Image& icon) {
  DCHECK_NE(tab_id, kDefaultTabId);
  declarative_icon_[tab_id][priority].push_back(icon);
}

void ExtensionAction::UndoDeclarativeSetIcon(int tab_id,
                                             int priority,
                                             const gfx::Image& icon) {
  std::vector<gfx::Image>& icons = declarative_icon_[tab_id][priority];
  for (std::vector<gfx::Image>::iterator it = icons.begin(); it != icons.end();
       ++it) {
    if (it->AsImageSkia().BackedBySameObjectAs(icon.AsImageSkia())) {
      icons.erase(it);
      return;
    }
  }
}

const gfx::Image ExtensionAction::GetDeclarativeIcon(int tab_id) const {
  if (declarative_icon_.find(tab_id) != declarative_icon_.end() &&
      !declarative_icon_.find(tab_id)->second.rbegin()->second.empty()) {
    return declarative_icon_.find(tab_id)->second.rbegin()->second.back();
  }
  return gfx::Image();
}

void ExtensionAction::ClearAllValuesForTab(int tab_id) {
  popup_url_.erase(tab_id);
  title_.erase(tab_id);
  icon_.erase(tab_id);
  badge_text_.erase(tab_id);
  badge_text_color_.erase(tab_id);
  badge_background_color_.erase(tab_id);
  is_visible_.erase(tab_id);
  // TODO(jyasskin): Erase the element from declarative_show_count_
  // when the tab's closed.  There's a race between the
  // LocationBarController and the ContentRulesRegistry on navigation,
  // which prevents me from cleaning everything up now.
}

extensions::IconImage* ExtensionAction::LoadDefaultIconImage(
    const extensions::Extension& extension,
    content::BrowserContext* browser_context) {
  if (default_icon_ && !default_icon_image_) {
    default_icon_image_.reset(new extensions::IconImage(
        browser_context, &extension, *default_icon(), ActionIconSize(),
        *GetDefaultIcon().ToImageSkia(), nullptr));
  }
  return default_icon_image_.get();
}

gfx::Image ExtensionAction::GetDefaultIconImage() const {
  // If we have a default icon, it should be loaded before trying to use it.
  DCHECK(!default_icon_image_ == !default_icon_);
  if (default_icon_image_)
    return default_icon_image_->image();

  if (placeholder_icon_image_.IsEmpty()) {
    // If the extension action redesign is enabled, we use a special placeholder
    // icon (with the first letter of the extension name) rather than the
    // default (puzzle piece).
    if (extensions::FeatureSwitch::extension_action_redesign()->IsEnabled()) {
      placeholder_icon_image_ =
          extensions::ExtensionIconPlaceholder::CreateImage(ActionIconSize(),
                                                            extension_name_);
    } else {
      placeholder_icon_image_ = GetDefaultIcon();
    }
  }

  return placeholder_icon_image_;
}

bool ExtensionAction::HasPopupUrl(int tab_id) const {
  return HasValue(popup_url_, tab_id);
}

bool ExtensionAction::HasTitle(int tab_id) const {
  return HasValue(title_, tab_id);
}

bool ExtensionAction::HasBadgeText(int tab_id) const {
  return HasValue(badge_text_, tab_id);
}

bool ExtensionAction::HasBadgeBackgroundColor(int tab_id) const {
  return HasValue(badge_background_color_, tab_id);
}

bool ExtensionAction::HasBadgeTextColor(int tab_id) const {
  return HasValue(badge_text_color_, tab_id);
}

bool ExtensionAction::HasIsVisible(int tab_id) const {
  return HasValue(is_visible_, tab_id);
}

bool ExtensionAction::HasIcon(int tab_id) const {
  return HasValue(icon_, tab_id);
}

void ExtensionAction::SetDefaultIconForTest(
    scoped_ptr<ExtensionIconSet> default_icon) {
  default_icon_ = std::move(default_icon);
}

void ExtensionAction::Populate(const extensions::Extension& extension,
                               const extensions::ActionInfo& manifest_data) {
  // If the manifest doesn't specify a title, set it to |extension|'s name.
  const std::string& title =
      !manifest_data.default_title.empty() ? manifest_data.default_title :
      extension.name();
  SetTitle(kDefaultTabId, title);
  SetPopupUrl(kDefaultTabId, manifest_data.default_popup_url);
  set_id(manifest_data.id);

  // Initialize the specified icon set.
  if (!manifest_data.default_icon.empty()) {
    default_icon_.reset(new ExtensionIconSet(manifest_data.default_icon));
  } else {
    // Fall back to the product icons if no action icon exists.
    const ExtensionIconSet& product_icons =
        extensions::IconsInfo::GetIcons(&extension);
    if (!product_icons.empty())
      default_icon_.reset(new ExtensionIconSet(product_icons));
  }
}

// Determines which icon would be returned by |GetIcon|, and returns its width.
int ExtensionAction::GetIconWidth(int tab_id) const {
  // If icon has been set, return its width.
  gfx::Image icon = GetValue(&icon_, tab_id);
  if (!icon.IsEmpty())
    return icon.Width();
  // If there is a default icon, the icon width will be set depending on our
  // action type.
  if (default_icon_)
    return ActionIconSize();

  // If no icon has been set and there is no default icon, we need favicon
  // width.
  return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_EXTENSIONS_FAVICON).Width();
}
