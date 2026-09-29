#ifndef MATH_H
#define MATH_H

#include "core.h"
#include "HandmadeMath.h"


#define V2 HMM_V2
#define V3 HMM_V3
#define V4 HMM_V4

typedef HMM_Vec2 vec2;
typedef HMM_Vec3 vec3;
typedef HMM_Vec4 vec4;
typedef HMM_Mat4 mat4;
typedef HMM_Quat quat;

#define V2_FMT      "[%.3f, %.3f]"
#define V3_FMT      "[%.3f, %.3f, %.3f]"
#define V4_FMT      "[%.3f, %.3f, %.3f, %.3f]"
#define V2_ARG(v)   (f64)(v).X, (f64)(v).Y
#define V3_ARG(v)   (f64)(v).X, (f64)(v).Y, (f64)(v).Z
#define V4_ARG(v)   (f64)(v).X, (f64)(v).Y, (f64)(v).Z, (f64)(v).W

#define lerp HMM_Lerp

inline f32 smoothstep(f32 t) { return t * t * (3.0f - 2.0f * t); }
inline f32 smootherstep(f32 t) { return t * t * t * (t * (6 * t - 15) + 10); }

/* out and back: 0 -> 1 -> 0 over the animation */
inline f32 outbackstep(f32 t) { return (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f); }

inline i32 round_i32(f32 x) { return (i32)(x + (x >= 0.0f ? 0.5f : -0.5f)); }
inline u32 round_u32(f32 x) { return (u32)(x + 0.5f); }




#endif
