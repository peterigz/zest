#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 
#define IMGUI_DISABLE_OBSOLETE_KEYIO 
#define IMGUI_ENABLE_TEST_ENGINE 
#define IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL 1

#define IM_VEC2_CLASS_EXTRA                                                 \
        bool operator==(ImVec2 &v1) { return v1.x == x && v1.y == y; }		\
        bool operator!=(ImVec2 &v1) { return v1.x != x || v1.y != y; }
#define ImDrawIdx unsigned int

