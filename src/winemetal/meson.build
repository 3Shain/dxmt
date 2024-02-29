winemetal_src = [
  'main.c',
  'exports.c',
  'metalcpp-impl.cpp',
  'dispatch-queue.cpp',
]

winemetal_ld_args      = ['-Wl,/entry:WineMetalEntry']
winemetal_link_depends = []

lib_winecrt0 = cc.find_library('winecrt0', dirs : [ join_paths(meson.source_root(), 'toolchains/wine/lib/wine/x86_64-windows') ])
lib_ntdll = cc.find_library('ntdll', dirs : [ join_paths(meson.source_root(), 'toolchains/wine/lib/wine/x86_64-windows') ])

winemetal_dll = shared_library('winemetal', winemetal_src,
  name_prefix         : '',
  dependencies        : [ lib_winecrt0, lib_ntdll ],
  include_directories : dxmt_include_path,
  install             : true,
  link_args           : winemetal_ld_args,
  link_depends        : [ winemetal_link_depends ],
)

winemetal_dep = declare_dependency(
  link_with           : [ winemetal_dll ],
  include_directories : [ dxmt_include_path ],
)

custom_target('postprocess_lib',
  input : winemetal_dll,
  output: 'winemetal.dll.postproc',
  command : [join_paths(meson.source_root(), 'toolchains/wine/bin/winebuild'), '--builtin', '@INPUT@'],
  depends : winemetal_dll,
  build_by_default : true
)

subdir('unix')