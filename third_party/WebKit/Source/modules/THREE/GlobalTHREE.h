// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalTHREE_h
#define GlobalTHREE_h

namespace blink {

class THREE;
class DOMWindow;
class WorkerGlobalScope;

class GlobalTHREE {
public:
    static THREE* three(DOMWindow&);
    static THREE* three(WorkerGlobalScope&);
};

} // namespace blink

#endif // GlobalTHREE_h
