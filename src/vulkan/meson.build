vulkan_sources += files(
    'vulkan_renderer.c',
    'vulkan_pass.c',
    'vulkan_pipeline.c',
    'vulkan_buffer.c',
    'vulkan_image.c',
    'vulkan_memory.c',
    'vulkan_texture.c',
)

# backs the DEBUG_BUILD-only pipeline vertex input validation
if get_option('buildtype').startswith('debug')
    vulkan_sources += files('vulkan_spirv.c')
endif
