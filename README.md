# Zest
## A Vulkan Pocket Renderer

Zest is a very simple single-header C library for drawing things on the screen with Vulkan. It has minimal dependencies, which are the Vulkan SDK and a single-header library bundle that includes some STB libraries and my memory allocator.

As it stands currently, this library is primarily for drawing 2D and 3D sprites, but it has plenty of flexibility for adding your custom draw routines. It's also straightforward to set up compute shaders.

I primarily made this because I'm developing TimelineFX - a particle effects library and editor - and needed a simple renderer with as little mess and as few dependencies as possible. I can use this library to demonstrate how to use TimelineFX with minimal setup required (TimelineFX is render-agnostic).

The best way to learn how to use it is to look at some of the examples in the repository. The examples come in two flavors: simple, which have no dependencies other than Vulkan, and GLFW, which shows you how to use GLFW to open a window and poll input. The GLFW examples also demonstrate how to use Dear ImGui (included as submodules).

![The Vaders example with Zest](https://www.rigzsoft.co.uk/media//VadersGameplay.jpg)

### Cloning the Repository

Simply clone the repository to your hard drive and ensure that the submodules (for some of the examples) are installed with `git submodule update --init --recursive`.

### How to Use

To use Zest, copy zest.h, lib_bundle.h, and zest.c into your project, include zest.h, and make sure that the project knows where to find vulkan-1.lib. You can also compile Zest as a library if you prefer that approach.

Initializing Zest looks like this:

```c
// Make a config struct where you can configure Zest with some options
zest_create_info_t create_info = zest_CreateInfo();

// Initialize Zest with the configuration
zest_Initialise(&create_info);
// Set the callback you want to use that will be called each frame.
zest_SetUserUpdateCallback(UpdateCallback);

// Start the Zest main loop
zest_Start();
```

### Using CMake

Zest includes CMakeLists so that you can use CMake to build Zest on your platform (only Mac and Windows currently). This will build a solution with all the example projects ready to go, but for the examples that use GLFW, you will have to locate the lib and header files for those during the CMake configuration.

There's also the option to install an example called zest-msdf-font-maker, which is a small program you can use to convert TTF fonts into ZTF fonts so that you can draw multi-channel signed distance fonts.

Sorry for the lack of documentation at this point, but I will be building on this in the future!
