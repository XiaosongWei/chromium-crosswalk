#include "config.h"
#include "THREE.h"
#include <android/log.h>
#define  LOG_TAG    "THREE"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

#define FUNC_NAME(a, b) a ## b
#define ATTRIBUTE_GETTER_CUSTOM(_class_) \
    void V8THREE:: FUNC_NAME(_class_, AttributeGetterCustom) (v8::FunctionCallbackInfo<v8::Value> const& info) { \
        v8::Isolate* isolate = info.GetIsolate(); \
        v8::Handle<v8::FunctionTemplate> functionTemplate = V8##_class_##B::domTemplate(isolate); \
        v8::Handle<v8::Function> v8Function = functionTemplate->GetFunction(); \
        v8SetReturnValue(info, v8Function); \
    }

//#include "THREE.hpp"
#include "bindings/modules/v8/V8THREE.h"
#include "bindings/modules/v8/V8Matrix4B.h"

namespace blink {


   //HashMap<void*, threeBlinkWrapper*> threeBlinkWrapperRepo::m_repoMap;

   void THREE::runTest()
   {
   }

   ATTRIBUTE_GETTER_CUSTOM(Matrix4)

   /*
   void V8THREE::ConcreteContactResultCallbackAttributeGetterCustom(v8::FunctionCallbackInfo<v8::Value> const& info)
   {
        v8::Isolate* isolate = info.GetIsolate();
        v8::Handle<v8::FunctionTemplate> functionTemplate = V8ConcreteContactResultCallbackB::domTemplate(isolate);
        v8::Handle<v8::Function> v8Function = functionTemplate->GetFunction();
        v8SetReturnValue(info, v8Function);
   }
   */

}

