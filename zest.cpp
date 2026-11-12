// Zest static library implementation
// This file compiles zest.h into a static library (.lib)

#define ZEST_IMPLEMENTATION
#define ZEST_VULKAN_IMPLEMENTATION
#ifdef ZEST_ENABLE_D3D12
#define ZEST_D3D12_IMPLEMENTATION
#endif
#include "zest.h"
