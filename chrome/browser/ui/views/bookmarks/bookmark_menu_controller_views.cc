// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"

#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using bookmarks::BookmarkNode;
using content::PageNavigator;
using views::MenuItemView;

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               PageNavigator* page_navigator,
                                               views::Widget* parent,
                                               const BookmarkNode* node,
                                               int start_child_index,
                                               bool for_drop)
    : menu_delegate_(new BookmarkMenuDelegate(browser, page_navigator, parent)),
      node_(node),
      observer_(NULL),
      for_drop_(for_drop),
      bookmark_bar_(NULL) {
  menu_delegate_->Init(this, NULL, node, start_child_index,
                       BookmarkMenuDelegate::HIDE_PERMANENT_FOLDERS,
                       BOOKMARK_LAUNCH_LOCATION_BAR_SUBFOLDER);
  menu_runner_.reset(new views::MenuRunner(
      menu_delegate_->menu(), for_drop ? views::MenuRunner::FOR_DROP : 0));
}

void BookmarkMenuController::RunMenuAt(BookmarkBarView* bookmark_bar) {
  bookmark_bar_ = bookmark_bar;
  views::MenuButton* menu_button = bookmark_bar_->GetMenuButtonForNode(node_);
  DCHECK(menu_button);
  views::MenuAnchorPosition anchor;
  bookmark_bar_->GetAnchorPositionForButton(menu_button, &anchor);
  gfx::Point screen_loc;
  views::View::ConvertPointToScreen(menu_button, &screen_loc);
  // Subtract 1 from the height to make the popup flush with the button border.
  gfx::Rect bounds(screen_loc.x(), screen_loc.y(), menu_button->width(),
                   menu_button->height() - 1);
  menu_delegate_->GetBookmarkModel()->AddObserver(this);
  // We only delete ourself after the menu completes, so we can safely ignore
  // the return value.
  ignore_result(menu_runner_->RunMenuAt(menu_delegate_->parent(),
                                        menu_button,
                                        bounds,
                                        anchor,
                                        ui::MENU_SOURCE_NONE));
}

void BookmarkMenuController::Cancel() {
  menu_delegate_->menu()->Cancel();
}

MenuItemView* BookmarkMenuController::menu() const {
  return menu_delegate_->menu();
}

MenuItemView* BookmarkMenuController::context_menu() const {
  return menu_delegate_->context_menu();
}

void BookmarkMenuController::SetPageNavigator(PageNavigator* navigator) {
  menu_delegate_->SetPageNavigator(navigator);
}

base::string16 BookmarkMenuController::GetTooltipText(int id,
                                                const gfx::Point& p) const {
  return menu_delegate_->GetTooltipText(id, p);
}

bool BookmarkMenuController::IsTriggerableEvent(views::MenuItemView* menu,
                                                const ui::Event& e) {
  return menu_delegate_->IsTriggerableEvent(menu, e);
}

void BookmarkMenuController::ExecuteCommand(int id, int mouse_event_flags) {
  menu_delegate_->ExecuteCommand(id, mouse_event_flags);
}

bool BookmarkMenuController::ShouldExecuteCommandWithoutClosingMenu(
      int id, const ui::Event& e) {
  return menu_delegate_->ShouldExecuteCommandWithoutClosingMenu(id, e);
}

bool BookmarkMenuController::GetDropFormats(
    MenuItemView* menu,
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return menu_delegate_->GetDropFormats(menu, formats, format_types);
}

bool BookmarkMenuController::AreDropTypesRequired(MenuItemView* menu) {
  return menu_delegate_->AreDropTypesRequired(menu);
}

bool BookmarkMenuController::CanDrop(MenuItemView* menu,
                                     const ui::OSExchangeData& data) {
  return menu_delegate_->CanDrop(menu, data);
}

int BookmarkMenuController::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return menu_delegate_->GetDropOperation(item, event, position);
}

int BookmarkMenuController::OnPerformDrop(MenuItemView* menu,
                                          DropPosition position,
                                          const ui::DropTargetEvent& event) {
  int result = menu_delegate_->OnPerformDrop(menu, position, event);
  if (for_drop_)
    delete this;
  return result;
}

bool BookmarkMenuController::ShowContextMenu(MenuItemView* source,
                                             int id,
                                             const gfx::Point& p,
                                             ui::MenuSourceType source_type) {
  return menu_delegate_->ShowContextMenu(source, id, p, source_type);
}

bool BookmarkMenuController::CanDrag(MenuItemView* menu) {
  return menu_delegate_->CanDrag(menu);
}

void BookmarkMenuController::WriteDragData(MenuItemView* sender,
                                           ui::OSExchangeData* data) {
  return menu_delegate_->WriteDragData(sender, data);
}

int BookmarkMenuController::GetDragOperations(MenuItemView* sender) {
  return menu_delegate_->GetDragOperations(sender);
}

void BookmarkMenuController::OnMenuClosed(views::MenuItemView* menu,
                                          views::MenuRunner::RunResult result) {
  delete this;
}

views::MenuItemView* BookmarkMenuController::GetSiblingMenu(
    views::MenuItemView* menu,
    const gfx::Point& screen_point,
    views::MenuAnchorPosition* anchor,
    bool* has_mnemonics,
    views::MenuButton** button) {
  if (!bookmark_bar_ || for_drop_)
    return NULL;
  gfx::Point bookmark_bar_loc(screen_point);
  views::View::ConvertPointFromScreen(bookmark_bar_, &bookmark_bar_loc);
  int start_index;
  const BookmarkNode* node = bookmark_bar_->GetNodeForButtonAtModelIndex(
      bookmark_bar_loc, &start_index);
  if (!node || !node->is_folder())
    return NULL;

  menu_delegate_->SetActiveMenu(node, start_index);
  *button = bookmark_bar_->GetMenuButtonForNode(node);
  bookmark_bar_->GetAnchorPositionForButton(*button, anchor);
  *has_mnemonics = false;
  return this->menu();
}

int BookmarkMenuController::GetMaxWidthForMenu(MenuItemView* view) {
  return menu_delegate_->GetMaxWidthForMenu(view);
}

void BookmarkMenuController::WillShowMenu(MenuItemView* menu) {
  menu_delegate_->WillShowMenu(menu);
}

void BookmarkMenuController::BookmarkModelChanged() {
  if (!menu_delegate_->is_mutating_model())
    menu()->Cancel();
}

BookmarkMenuController::~BookmarkMenuController() {
  menu_delegate_->GetBookmarkModel()->RemoveObserver(this);
  if (observer_)
    observer_->BookmarkMenuControllerDeleted(this);
}
