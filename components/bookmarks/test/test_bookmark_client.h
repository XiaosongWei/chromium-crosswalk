// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
#define COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/bookmarks/browser/bookmark_client.h"

namespace bookmarks {

class BookmarkModel;

class TestBookmarkClient : public BookmarkClient {
 public:
  TestBookmarkClient();
  ~TestBookmarkClient() override;

  // Create a BookmarkModel using this object as its client. The returned
  // BookmarkModel* is owned by the caller.
  scoped_ptr<BookmarkModel> CreateModel();

  // Sets the list of extra nodes to be returned by the next call to
  // CreateModel() or GetLoadExtraNodesCallback().
  void SetExtraNodesToLoad(BookmarkPermanentNodeList extra_nodes);

  // Returns the current extra_nodes, set via SetExtraNodesToLoad().
  const std::vector<BookmarkPermanentNode*>& extra_nodes() {
    return extra_nodes_;
  }

  // Returns true if |node| is one of the |extra_nodes_|.
  bool IsExtraNodeRoot(const BookmarkNode* node);

  // Returns true if |node| belongs to the tree of one of the |extra_nodes_|.
  bool IsAnExtraNode(const BookmarkNode* node);

 private:
  // BookmarkClient:
  bool IsPermanentNodeVisible(const BookmarkPermanentNode* node) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  LoadExtraCallback GetLoadExtraNodesCallback() override;
  bool CanSetPermanentNodeTitle(const BookmarkNode* permanent_node) override;
  bool CanSyncNode(const BookmarkNode* node) override;
  bool CanBeEditedByUser(const BookmarkNode* node) override;

  // Helpers for GetLoadExtraNodesCallback().
  static BookmarkPermanentNodeList LoadExtraNodes(
      BookmarkPermanentNodeList extra_nodes,
      int64_t* next_id);

  BookmarkPermanentNodeList extra_nodes_to_load_;
  std::vector<BookmarkPermanentNode*> extra_nodes_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkClient);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_TEST_TEST_BOOKMARK_CLIENT_H_
