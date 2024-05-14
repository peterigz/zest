@echo off
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V mesh_instance_custom.frag -o mesh_instance_custom_frag.spv
E:/VulkanSDK/1.3.261.1/Bin/glslangValidator.exe -V mesh_instance_custom.vert -o mesh_instance_custom_vert.spv

E:/VulkanSDK/1.3.261.1/Bin/spirv-link mesh_instance_custom_vert.spv mesh_instance_custom_frag.spv -o ../spv/mesh_instance_custom.spv

del mesh_instance_custom_vert.spv
del mesh_instance_custom_frag.spv

