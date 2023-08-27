E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V image.frag -o image_frag.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V image_alpha.frag -o image_alpha_frag.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V instance.vert -o instance_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V billboard.vert -o billboard_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V imgui.vert -o imgui_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V imgui.frag -o imgui_frag.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V swap.vert -o swap_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V swap.frag -o swap_frag.spv

E:/VulkanSDK/1.2.176.1/Bin32/spirv-link instance_vert.spv image_frag.spv -o ../spv/instance.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link instance_vert.spv image_alpha_frag.spv -o ../spv/instance_alpha.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link billboard_vert.spv image_frag.spv -o ../spv/billboard.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link billboard_vert.spv image_alpha_frag.spv -o ../spv/billboard_alpha.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link swap_vert.spv swap_frag.spv -o ../spv/swap.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link imgui_vert.spv imgui_frag.spv -o ../spv/imgui.spv

del image_frag.spv
del image_alpha_frag.spv
del instance_vert.spv
del billboard_vert.spv
del imgui_vert.spv
del imgui_frag.spv
del swap_vert.spv
del swap_frag.spv

xcopy "../spv" "../examples/zest-imgui-template/spv" /y
xcopy "../spv" "../examples/zest-instance-layers/spv" /y
xcopy "../spv" "../examples/zest-minimal-template/spv" /y
xcopy "../spv" "../examples/zest-render-targets/spv" /y
