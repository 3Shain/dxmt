nvngx_src = [
  'nvngx.cpp',
]

nvngx_ld_args      = []
nvngx_link_depends = []

nvngx_dll = shared_library('nvngx', nvngx_src,
  name_prefix         : '',
  dependencies        : [ util_dep ],
  include_directories : [ dxmt_include_path ],
  install             : true,
  link_args           : nvngx_ld_args,
  link_depends        : [ nvngx_link_depends ],
)

# always build as a wine-builtin dll
custom_target('postprocess_libnvngx',
  input : nvngx_dll,
  output: 'nvngx.dll.postproc',
  command : [ winebuild, '--builtin', '@INPUT@' ],
  depends : nvngx_dll,
  build_by_default : true
)
