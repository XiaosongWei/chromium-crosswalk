#ifndef MATRIX4B_H
#define MATRIX4B_H

//#include "../impl/Math/Matrix4.h"
#include "threeBlinkWrapper.h"
#include "core/dom/DOMTypedArray.h"


namespace blink{

class Matrix4B : public threeBlinkWrapper {
   DEFINE_WRAPPERTYPEINFO();
public:
    static Matrix4B* create();
    virtual ~Matrix4B();
    Matrix4B();
    void set(float n11, float n12, float n13, float n14, float n21, float n22, float n23, float n24, 
        float n31, float n32, float n33, float n34, float n41, float n42, float n43, float n44);
    
    Matrix4B* multiplyMatrices(Matrix4B* a, Matrix4B* b);

    DOMFloat32Array* elements() { return m_elements.get();}
    void set_elements(PassRefPtr<DOMFloat32Array> elements) { m_elements = elements; }
    RefPtr<DOMFloat32Array> m_elements;
    
};
    

}

#endif
