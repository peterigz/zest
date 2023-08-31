E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V image_alpha.frag -o image_alpha_frag.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V instance.vert -o instance_vert.spv

E:/VulkanSDK/1.2.176.1/Bin32/spirv-link instance_vert.spv image_alpha_frag.spv -o ../spv/sdf.spv

del image_alpha_frag.spv
del instance_vert.spv
