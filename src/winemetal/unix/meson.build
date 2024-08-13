winemetal_unix_src = [
  'unix.c',
]

winemetal_unix_ld_args = [
  '-install_name', 
  '@rpath/winemetal.so',
  join_paths(meson.source_root(), 'toolchains/wine/lib/wine/x86_64-unix/winemac.so'),
  '-weak_framework', 'CoreFoundation', '-weak_framework', 'Metal', 
  '-framework', 'MetalFX',
]
winemetal_unix_link_depends = []

winemetal_unix_dll = shared_library('winemetal', winemetal_unix_src,
  name_prefix         : '',
  dependencies        : [ airconv_dep_darwin ],
  include_directories : dxmt_include_path,
  c_args              : ['-ObjC'],
  link_args           : winemetal_unix_ld_args,
  native              : true,
  name_suffix         : 'so',
  install_rpath       : '',
  build_rpath         : '@loader_path/:@loader_path/../../',
  override_options    : { 'b_asneeded' : false },
)