// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/THREE/GlobalTHREE.h"

#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/THREE/THREE.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/DatabaseIdentifier.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

template <typename T>
class GlobalTHREEImpl final : public NoBaseWillBeGarbageCollectedFinalized<GlobalTHREEImpl<T>>, public WillBeHeapSupplement<T> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(GlobalTHREEImpl);
public:
    static GlobalTHREEImpl& from(T& supplementable, ExecutionContext* executionContext)
    {
        GlobalTHREEImpl* supplement = static_cast<GlobalTHREEImpl*>(WillBeHeapSupplement<T>::from(supplementable, name()));
        if (!supplement) {
            supplement = new GlobalTHREEImpl();
            WillBeHeapSupplement<T>::provideTo(supplementable, name(), adoptPtrWillBeNoop(supplement));
        }
        return *supplement;
    }

    ~GlobalTHREEImpl()
    {

    }

    THREE* three(T& fetchingScope)
    {
        if(!m_three)
            m_three = THREE::create();
        return m_three;
    }

    // Promptly dispose of associated THREE.
    EAGERLY_FINALIZE();
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_three);
        WillBeHeapSupplement<T>::trace(visitor);
    }

private:
    GlobalTHREEImpl()
    {
    }

    static const char* name() { return "THREE"; }

    PersistentWillBeMember<THREE> m_three;
};

} // namespace

THREE* GlobalTHREE::three(DOMWindow& window)
{
    return GlobalTHREEImpl<LocalDOMWindow>::from(toLocalDOMWindow(window), window.executionContext()).three(toLocalDOMWindow(window));
}

THREE* GlobalTHREE::three(WorkerGlobalScope& worker)
{
    return GlobalTHREEImpl<WorkerGlobalScope>::from(worker, worker.executionContext()).three(worker);
}

} // namespace blink
