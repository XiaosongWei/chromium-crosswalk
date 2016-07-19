#ifndef THREE_BASEOBJ_H
#define THREE_BASEOBJ_H


#define USE_NEON

#ifdef USE_NEON
#include <arm_neon.h>
#endif


namespace three{




class BaseObj {

public:
    BaseObj()
    {
        refCount = 1;
        wrapper = NULL;
    }
    virtual ~BaseObj() {}
    
    void setWrapper(void* wrapper)
    {
        this->wrapper = wrapper;
      
    }

    void* getWrapper()
    {
       return wrapper;
    }

    void ref() {
        refCount++;
    }
    void deref() {
        refCount--;
        if(refCount <=1)
            delete this;
    }
    
private:
    unsigned int refCount;
    void* wrapper;
};

}
#endif

