d3d11_res = wrc_generator.process('version.rc')

d3d11_src = [
  'd3d11_buffer.cpp',
  'd3d11_class_linkage.cpp',
  'd3d11_context_state.cpp',
  'd3d11_context.cpp',
  'd3d11_device.cpp',
  'd3d11_input_layout.cpp',
  'd3d11_inspection.cpp',
  'd3d11_query.cpp',
  'd3d11_resource.cpp',
  'd3d11_shader.cpp',
  'd3d11_state_object.cpp',
  'd3d11_swapchain.cpp',
  'd3d11_texture.cpp',
  'd3d11_texture1d.cpp',
  'd3d11_texture2d.cpp',
  'd3d11.cpp',
  'mtld11_resource.cpp',
  'd3d11_names.cpp',
]

d3d11_ld_args      = []
d3d11_link_depends = []

d3d11_dll = shared_library('d3d11', d3d11_src, d3d11_res,
  name_prefix         : '',
  dependencies        : [ dxgi_dep, 
  DXBCParser_dep,
  airconv_dep,
  dxmt_dep, winemetal_dep ],
  include_directories : dxmt_include_path,
  install             : true,
  link_args           : d3d11_ld_args,
  link_depends        : [ d3d11_link_depends ],
)

d3d11_dep = declare_dependency(
  link_with           : [ d3d11_dll ],
  include_directories : [ dxmt_include_path ],
)