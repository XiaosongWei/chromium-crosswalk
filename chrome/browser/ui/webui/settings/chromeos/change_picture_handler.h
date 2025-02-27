// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/webui/settings/md_settings_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_manager {
class User;
}

namespace chromeos {

namespace settings {

// ChromeOS user image settings page UI handler.
class ChangePictureHandler : public ::settings::SettingsPageUIHandler,
                             public ui::SelectFileDialog::Listener,
                             public content::NotificationObserver,
                             public ImageDecoder::ImageRequest,
                             public CameraPresenceNotifier::Observer {
 public:
  ChangePictureHandler();
  ~ChangePictureHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // CameraPresenceNotifier::Observer implementation:
  void OnCameraPresenceCheckDone(bool is_camera_present) override;

 private:
  // Sends list of available default images to the page.
  void SendDefaultImages();

  // Sends current selection to the page.
  void SendSelectedImage();

  // Sends the profile image to the page. If |should_select| is true then
  // the profile image element is selected.
  void SendProfileImage(const gfx::ImageSkia& image, bool should_select);

  // Starts profile image update and shows the last downloaded profile image,
  // if any, on the page. Shouldn't be called before |SendProfileImage|.
  void UpdateProfileImage();

  // Sends previous user image to the page.
  void SendOldImage(const std::string& image_url);

  // Starts camera presence check.
  void CheckCameraPresence();

  // Updates UI with camera presence state.
  void SetCameraPresent(bool present);

  // Opens a file selection dialog to choose user image from file.
  void HandleChooseFile(const base::ListValue* args);

  // Handles 'take-photo' button click.
  void HandleTakePhoto(const base::ListValue* args);

  // Handles photo taken with WebRTC UI.
  void HandlePhotoTaken(const base::ListValue* args);

  // Handles 'discard-photo' button click.
  void HandleDiscardPhoto(const base::ListValue* args);

  // Gets the list of available user images and sends it to the page.
  void HandleGetAvailableImages(const base::ListValue* args);

  // Handles page initialized event.
  void HandlePageInitialized(const base::ListValue* args);

  // Selects one of the available images as user's.
  void HandleSelectImage(const base::ListValue* args);

  // SelectFileDialog::Delegate implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Sets user image to photo taken from camera.
  void SetImageFromCamera(const gfx::ImageSkia& photo);

  // Returns handle to browser window or NULL if it can't be found.
  gfx::NativeWindow GetBrowserWindow() const;

  // Overriden from ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Returns user related to current WebUI. If this user doesn't exist,
  // returns active user.
  const user_manager::User* GetUser() const;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Previous user image from camera/file and its data URL.
  gfx::ImageSkia previous_image_;
  std::string previous_image_url_;

  // Index of the previous user image.
  int previous_image_index_;

  // Last user photo, if taken.
  gfx::ImageSkia user_photo_;

  // Data URL for |user_photo_|.
  std::string user_photo_data_url_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<CameraPresenceNotifier, ChangePictureHandler> camera_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChangePictureHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CHANGE_PICTURE_HANDLER_H_
