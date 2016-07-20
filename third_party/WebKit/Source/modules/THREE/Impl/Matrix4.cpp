#include "Matrix4.h"

namespace three {

Matrix4* Matrix4::multiplyMatrices(Matrix4* a, Matrix4* b)
{

    float* baseA = (float*) a->elements;
    float* baseB = (float*) b->elements;
    float* baseR = (float*) elements;

#ifdef USE_NEON

    float32x2_t* baseBV = (float32x2_t*) baseB;
    float32x4_t* baseAV = (float32x4_t*) baseA;
    float32x4_t* baseRV = (float32x4_t*) baseR;

    float32x4_t a1 = baseAV[0];
    float32x4_t a2 = baseAV[1];
    float32x4_t a3 = baseAV[2];
    float32x4_t a4 = baseAV[3];

    float32x2_t b11 = baseBV[0];
    float32x2_t b12 = baseBV[1];
    float32x2_t b21 = baseBV[2];
    float32x2_t b22 = baseBV[3]; 
    float32x2_t b31 = baseBV[4];
    float32x2_t b32 = baseBV[5];
    float32x2_t b41 = baseBV[6];
    float32x2_t b42 = baseBV[7];

    float32x4_t  r1, r2, r3, r4;

    //interlace instructions to use CPU pipeline
    r1 = vmulq_lane_f32(a1, b11, 0);
    r2 = vmulq_lane_f32(a1, b21, 0);
    r3 = vmulq_lane_f32(a1, b31, 0);
    r4 = vmulq_lane_f32(a1, b41, 0);
    
    r1 = vmlaq_lane_f32(r1, a2, b11, 1);
    r2 = vmlaq_lane_f32(r2, a2, b21, 1);
    r3 = vmlaq_lane_f32(r3, a2, b31, 1);
    r4 = vmlaq_lane_f32(r4, a2, b41, 1);
    
    r1 = vmlaq_lane_f32(r1, a3, b12, 0);
    r2 = vmlaq_lane_f32(r2, a3, b22, 0);
    r3 = vmlaq_lane_f32(r3, a3, b32, 0);
    r4 = vmlaq_lane_f32(r4, a3, b42, 0);
    
    r1 = vmlaq_lane_f32(r1, a4, b12, 1);
    r2 = vmlaq_lane_f32(r2, a4, b12, 1);
    r3 = vmlaq_lane_f32(r3, a4, b32, 1);
    r4 = vmlaq_lane_f32(r4, a4, b42, 1);

    baseRV[0] = r1;
    baseRV[1] = r2;
    baseRV[2] = r3;
    baseRV[3] = r4;
#else

    var a11 = baseA[ 0 ], a12 = baseA[ 4 ], a13 = baseA[ 8 ], a14 = baseA[ 12 ];
    var a21 = baseA[ 1 ], a22 = baseA[ 5 ], a23 = baseA[ 9 ], a24 = baseA[ 13 ];
	var a31 = baseA[ 2 ], a32 = baseA[ 6 ], a33 = baseA[ 10 ], a34 = baseA[ 14 ];
	var a41 = baseA[ 3 ], a42 = baseA[ 7 ], a43 = baseA[ 11 ], a44 = baseA[ 15 ];

	var b11 = baseB[ 0 ], b12 = baseB[ 4 ], b13 = baseB[ 8 ], b14 = baseB[ 12 ];
	var b21 = baseB[ 1 ], b22 = baseB[ 5 ], b23 = baseB[ 9 ], b24 = baseB[ 13 ];
	var b31 = baseB[ 2 ], b32 = baseB[ 6 ], b33 = baseB[ 10 ], b34 = baseB[ 14 ];
	var b41 = baseB[ 3 ], b42 = baseB[ 7 ], b43 = baseB[ 11 ], b44 = baseB[ 15 ];

	baseR[ 0 ] = a11 * b11 + a12 * b21 + a13 * b31 + a14 * b41;
	baseR[ 4 ] = a11 * b12 + a12 * b22 + a13 * b32 + a14 * b42;
	baseR[ 8 ] = a11 * b13 + a12 * b23 + a13 * b33 + a14 * b43;
	baseR[ 12 ] = a11 * b14 + a12 * b24 + a13 * b34 + a14 * b44;

	baseR[ 1 ] = a21 * b11 + a22 * b21 + a23 * b31 + a24 * b41;
	baseR[ 5 ] = a21 * b12 + a22 * b22 + a23 * b32 + a24 * b42;
	baseR[ 9 ] = a21 * b13 + a22 * b23 + a23 * b33 + a24 * b43;
	baseR[ 13 ] = a21 * b14 + a22 * b24 + a23 * b34 + a24 * b44;

	baseR[ 2 ] = a31 * b11 + a32 * b21 + a33 * b31 + a34 * b41;
	baseR[ 6 ] = a31 * b12 + a32 * b22 + a33 * b32 + a34 * b42;
	baseR[ 10 ] = a31 * b13 + a32 * b23 + a33 * b33 + a34 * b43;
	baseR[ 14 ] = a31 * b14 + a32 * b24 + a33 * b34 + a34 * b44;

	baseR[ 3 ] = a41 * b11 + a42 * b21 + a43 * b31 + a44 * b41;
	baseR[ 7 ] = a41 * b12 + a42 * b22 + a43 * b32 + a44 * b42;
	baseR[ 11 ] = a41 * b13 + a42 * b23 + a43 * b33 + a44 * b43;
	baseR[ 15 ] = a41 * b14 + a42 * b24 + a43 * b34 + a44 * b44;

#endif

return this;

}


}
