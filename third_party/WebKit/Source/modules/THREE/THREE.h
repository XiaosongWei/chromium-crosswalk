#ifndef THREE_h
#define THREE_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/WTFString.h"

#include "THREE_class.hpp"
namespace blink {
    class THREE : public GarbageCollectedFinalized<THREE>, public ScriptWrappable {
        DEFINE_WRAPPERTYPEINFO();
        public:
            static THREE* create()
            {
                return new THREE();
            }
            ~THREE()  {    }

            void runTest();

            DEFINE_INLINE_TRACE() {}

        private:
            THREE()  {    }
};
}

#endif

