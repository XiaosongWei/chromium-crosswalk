// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Copyright (c) 2015 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/three/DOMWindowTHREE.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/three/THREE.h"

namespace blink {

DOMWindowTHREE::DOMWindowTHREE(LocalDOMWindow& window)
{
}

DOMWindowTHREE::DOMWindowTHREE(WorkerGlobalScope& worker)
{
}

DOMWindowTHREE::~DOMWindowTHREE()
{
}

const char* DOMWindowTHREE::supplementName()
{
    return "DOMWindowTHREE";
}

DOMWindowTHREE& DOMWindowTHREE::from(LocalDOMWindow& window)
{
    DOMWindowTHREE* three = static_cast<DOMWindowTHREE*>(WillBeHeapSupplement<LocalDOMWindow>::from(window, supplementName()));
    if (!three) {
        three = new DOMWindowTHREE(window);
        WillBeHeapSupplement<LocalDOMWindow>::provideTo(window, supplementName(), adoptPtrWillBeNoop(three));
    }
    return *three;
}

DOMWindowTHREE& DOMWindowTHREE::from(WorkerGlobalScope& worker)
{
    DOMWindowTHREE* three = static_cast<DOMWindowTHREE*>(WillBeHeapSupplement<WorkerGlobalScope>::from(woker, supplementName()));
    if (!three) {
        three = new DOMWindowTHREE(worker);
        WillBeHeapSupplement<WorkerGlobalScope::provideTo(worker, supplementName(), adoptPtrWillBeNoop(three));
    }
    return *three;
}


THREE* DOMWindowTHREE::three(DOMWindow& window)
{
    return DOMWindowTHREE::from(toLocalDOMWindow(window)).three();
}

THREE* DOMWindowTHREE::three(WorkerGlobalScope& worker)
{
    return DOMWindowTHREE::from(worker).three();
}


THREE* DOMWindowTHREE::three() const
{
    if (!m_three && frame())
        m_three = THREE::create();
    return m_three.get();
}

} // namespace blink
