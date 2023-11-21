E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V image.frag -o image_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V image_alpha.frag -o image_alpha_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V font.frag -o font_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V instance.vert -o instance_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V billboard.vert -o billboard_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V imgui.vert -o imgui_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V imgui.frag -o imgui_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V mesh.vert -o mesh_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V swap.vert -o swap_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V swap.frag -o swap_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V shape_instance.vert -o shape_instance_vert.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V shape_instance.frag -o shape_instance_frag.spv

E:/VulkanSDK/1.3.261.1/Bin/spirv-link instance_vert.spv image_frag.spv -o ../spv/instance.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link shape_instance_vert.spv shape_instance_frag.spv -o ../spv/shape_instance.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link mesh_vert.spv image_frag.spv -o ../spv/mesh.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link instance_vert.spv font_frag.spv -o ../spv/font_instance.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link instance_vert.spv image_alpha_frag.spv -o ../spv/instance_alpha.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link billboard_vert.spv image_frag.spv -o ../spv/billboard.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link billboard_vert.spv image_alpha_frag.spv -o ../spv/billboard_alpha.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link swap_vert.spv swap_frag.spv -o ../spv/swap.spv
E:/VulkanSDK/1.3.261.1/Bin/spirv-link imgui_vert.spv imgui_frag.spv -o ../spv/imgui.spv

del image_frag.spv
del image_alpha_frag.spv
del font_frag.spv
del instance_vert.spv
del billboard_vert.spv
del mesh_vert.spv
del imgui_vert.spv
del imgui_frag.spv
del swap_vert.spv
del swap_frag.spv
del shape_instance_vert.spv
del shape_instance_frag.spv
