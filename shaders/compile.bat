E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V image.frag -o image_frag.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V instance.vert -o instance_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V swap.vert -o swap_vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V swap.frag -o swap_frag.spv

E:/VulkanSDK/1.2.176.1/Bin32/spirv-link instance_vert.spv image_frag.spv -o ../spv/instance.spv
E:/VulkanSDK/1.2.176.1/Bin32/spirv-link swap_vert.spv swap_frag.spv -o ../spv/swap.spv

del image_frag.spv
del instance_vert.spv
del swap_vert.spv
del swap_frag.spv
