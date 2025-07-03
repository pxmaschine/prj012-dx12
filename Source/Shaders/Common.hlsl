#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#include "Shared.h"

// Resources are bound to tables grouped by update frequency, to optimize performance
// Tables are also in order from most frequently to least frequently updated, to optimize performance
#define per_object_space   space0
#define per_material_space space1
#define per_pass_space     space2
#define per_frame_space    space3

#endif