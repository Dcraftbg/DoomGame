#pragma once
#include <math.h>
typedef float (*easeFunction)(float);
float easeLinear(float x);
float easeInCubic(float x);
float easeOutSine(float x);
float easeInOutQuad(float x);
#ifdef EASE_IMPLEMENTATION
float easeLinear(float x) {
	return x;
}
float easeInCubic(float x) {
	return x * x * x;
}
float easeOutSine(float x) {
	return sinf((x * PI) / 2);
}
float easeInOutQuad(float x) {
	return x < 0.5f ? 2 * x * x : 1 - powf(-2 * x + 2, 2) / 2.0f;
}
#endif