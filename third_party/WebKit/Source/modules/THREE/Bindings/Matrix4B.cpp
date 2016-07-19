
#include "config.h"
#include "../Impl/Matrix4.h"
#include "Matrix4B.h"

using namespace three;

namespace blink{


Matrix4B:: Matrix4B() {
    
}

Matrix4B:: ~Matrix4B() {
}

Matrix4B* Matrix4B::  create() {
    Matrix4B *wrapper = new Matrix4B();
    
    wrapper->m_elements = DOMFloat32Array::create(16);
    Matrix4 *impl = new Matrix4((float*)(wrapper->m_elements->baseAddress()));
    wrapper->setImpl(impl);
    
    return wrapper;
}

void Matrix4B:: set(float n11, float n12, float n13, float n14, float n21, float n22, float n23, float n24, 
        float n31, float n32, float n33, float n34, float n41, float n42, float n43, float n44) {
    
     Matrix4 *impl = (Matrix4 *)m_impl;
     impl->set(n11, n12, n13, n14, n21, n22, n23, n24, n31, n32, n33, n34, n41, n42, n43, n44);    
}

Matrix4B* Matrix4B:: multiplyMatrices(Matrix4B* a, Matrix4B* b)
{
     Matrix4* impl = (Matrix4 *)m_impl;
     Matrix4* a_impl = (Matrix4 *) a->getImpl();
     Matrix4* b_impl = (Matrix4 *) b->getImpl();
     impl->multiplyMatrices(a_impl, b_impl);
     return this;
}




}

