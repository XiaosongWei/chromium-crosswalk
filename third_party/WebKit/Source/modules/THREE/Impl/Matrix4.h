#ifndef THREE_MATRIX4_H
#define THREE_MATRIX4_H

#include "threeBaseObj.h"
#include <stdlib.h>


namespace three {

static const float identity[16] = {1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0};

class Matrix4: public BaseObj {

public:
    
    Matrix4(float* elementAddress = NULL)
    {
         elements    = elementAddress;
         m_ownedData = false;
         if(elements == NULL)
         {
             elements = (float*) malloc(16*sizeof(float));
             m_ownedData = true;
         }
         memcpy(elements, identity, 16*sizeof(float));
    }
    
    virtual ~Matrix4()
    {
        if(m_ownedData && elements)
            free(elements);
    }
    
    float* elements;
    bool m_ownedData;

    Matrix4* multiplyMatrices(Matrix4* a, Matrix4* b);
    
    void set(float n11, float n12, float n13, float n14, float n21, float n22, float n23, float n24,
             float n31, float n32, float n33, float n34, float n41, float n42, float n43, float n44) {

     elements[0] = n11;
     elements[4] = n12;
     elements[8] = n13;
     elements[12] = n14;

     elements[1] = n21;
     elements[5] = n22;
     elements[9] = n23;
     elements[13] = n24;

     elements[2] = n31;
     elements[6] = n32;
     elements[10] = n33;
     elements[14] = n34;

     elements[3] = n41;
     elements[7] = n42;
     elements[11] = n43;
     elements[15] = n44;
    }
};

}

#endif

