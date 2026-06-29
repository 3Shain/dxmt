d3d12_src = [
  'd3d12.cpp',
  'd3d12_buffer.cpp',
  'd3d12_command_allocator.cpp',
  'd3d12_command_list.cpp',
  'd3d12_command_queue.cpp',
  'd3d12_device.cpp',
  'd3d12_heap.cpp',
  'd3d12_root_signature.cpp',
  'd3d12_texture.cpp',
  '../d3d10/d3d10_blob.cpp',
]

d3d12_ld_args      = ['-L' + windows_native_install_dir] # a nice trick to force relink
d3d12_link_depends = []

d3d12_dll = shared_library('d3d12', d3d12_src,
  name_prefix         : '',
  dependencies        : [ dxgi_dep, dxmt_dep, airconv_forward_dep, util_dep, winemetal_dep, dxbc_parser_dep ],
  include_directories : [ dxmt_include_path ],
  install             : true,
  install_dir         : windows_native_install_dir,
  vs_module_defs      : 'd3d12.def',
  link_args           : d3d12_ld_args,
  link_depends        : [ d3d12_link_depends ],
)

d3d12_dep = declare_dependency(
  link_with           : [ d3d12_dll ],
  include_directories : [ dxmt_include_path, include_directories('.') ],
)

if wine_builtin_dll
custom_target('postprocess_libd3d12',
  input : d3d12_dll,
  output: 'd3d12.dll.postproc',
  command : [ winebuild, '--builtin', '@INPUT@' ],
  depends : d3d12_dll,
  build_by_default : true
)
endif
