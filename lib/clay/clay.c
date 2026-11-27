#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-variable"
#define CLAY_IMPLEMENTATION
#include "clay.h"
#pragma GCC diagnostic pop

const int32_t *
clay_open_elements(int32_t *n)
{
	Clay_Context *context = Clay_GetCurrentContext();

	*n = context->openLayoutElementStack.length;
	return context->openLayoutElementStack.internalArray;
}
