E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V particle.comp -o ../spv/particle_comp.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V particle.vert -o particle.vert.spv
E:/VulkanSDK/1.2.176.1/Bin32/glslangValidator.exe -V particle.frag -o particle.frag.spv

E:/VulkanSDK/1.2.176.1/Bin32/spirv-link particle.vert.spv particle.frag.spv -o ../spv/particle.spv

del particle.vert.spv
del particle.frag.spv
