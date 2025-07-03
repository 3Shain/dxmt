winemetal_src = [
  'nativemetal_thunks.c'
]

winemetal_ld_args = [
  '-install_name',
  '@rpath/nativemetal.dylib',
  '-weak_framework', 'CoreFoundation', '-weak_framework', 'Metal',
  '-framework', 'MetalFX', '-weak_framework', 'Foundation',
  '-weak_framework', 'CoreGraphics', '-weak_framework', 'QuartzCore',
  '-weak_framework', 'ColorSync',   '-weak_framework', 'Cocoa',
]
winemetal_link_depends = []

winemetal_dll = shared_library('nativemetal', winemetal_src,
  name_prefix         : '',
  dependencies        : [ airconv_dep_darwin ],
  include_directories : dxmt_include_path,
  c_args              : ['-ObjC'],
  link_args           : winemetal_ld_args,
  native              : true,
  name_suffix         : 'so',
  install_rpath       : '',
  build_rpath         : '@loader_path/:@loader_path/../../',
  override_options    : { 'b_asneeded' : false },
)

winemetal_dep = declare_dependency(
  link_with           : [ winemetal_dll ],
  include_directories : [ include_directories('.') ],
)
