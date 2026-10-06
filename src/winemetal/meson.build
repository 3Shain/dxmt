winemetal_src = [
  'main.c',
  'airconv_thunks.c',
  'winemetal_thunks.c'
]

winemetal_ld_args = []

winemetal_link_depends = []

winemetal_dll = shared_library('winemetal', winemetal_src,
  name_prefix         : '',
  dependencies        : [ lib_winecrt0, lib_ntdll, lib_dbghelp, airconv_forward_dep ],
  include_directories : dxmt_include_path,
  install             : true,
  install_dir         : windows_builtin_install_dir,
  link_args           : winemetal_ld_args,
  link_depends        : [ winemetal_link_depends ],
)

winemetal_dep = declare_dependency(
  link_with           : [ winemetal_dll ],
  include_directories : [ include_directories('.') ],
)

custom_target('postprocess_lib',
  input : winemetal_dll,
  output: 'winemetal.dll.postproc',
  command : [ winebuild, '--builtin', '@INPUT@' ],
  depends : winemetal_dll,
  build_by_default : true
)

if cpu_family == 'x86_64' or cpu_family == 'aarch64'
  subdir('unix')
endif
