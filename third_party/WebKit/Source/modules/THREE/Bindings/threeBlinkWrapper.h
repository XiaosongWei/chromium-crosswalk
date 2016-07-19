#ifndef THREE_BLINK_WRAPPER_H
#define THREE_BLINK_WRAPPER_H

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include "../Impl/threeBaseObj.h"


namespace blink {


class threeBlinkWrapper : public RefCounted<threeBlinkWrapper>, public ScriptWrappable {

    DEFINE_WRAPPERTYPEINFO();

public:
    threeBlinkWrapper() {
        m_impl = NULL;
     }
    virtual ~threeBlinkWrapper() {
        if(m_impl)
        {
            //if(m_isOwner)
            //   delete m_impl;

            m_impl->setWrapper(NULL);
            m_impl->deref();
        }
        m_impl = NULL;
     }

    void  setImpl(three::BaseObj* impl) {
        m_impl = impl;
        impl->ref();
        impl->setWrapper((void*) this);
     }


    three::BaseObj* getImpl() { return m_impl; }

    void setRef(String name, threeBlinkWrapper* pObj) {
        if(!m_refedWrappersMap.contains(name))
            m_refedWrappersMap.set(name, pObj); //Incerate reffernces by Refpr
    }
    void removeRef(String name) {
        if(m_refedWrappersMap.contains(name))
            m_refedWrappersMap.remove(name);   //Decreate references
    }

    void setRef(threeBlinkWrapper* pObj) {
        if(!m_refedWrappersVector.contains(pObj))
            m_refedWrappersVector.append(pObj);
    }
    void removeRef(threeBlinkWrapper* pObj) {
        if(m_refedWrappersVector.contains(pObj))
            m_refedWrappersVector.remove(m_refedWrappersVector.find(pObj));
    }

protected:
    three::BaseObj* m_impl; // Should  the implementation also  refcounted? as in COCOS2D?

    HashMap<String, RefPtr<threeBlinkWrapper>> m_refedWrappersMap;
    Vector<RefPtr<threeBlinkWrapper>> m_refedWrappersVector;

};



}

#endif
