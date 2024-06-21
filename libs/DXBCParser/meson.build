DXBCParser_src = files([
  'DXBCUtils.cpp',
  'BlobContainer.cpp',
  'ShaderBinary.cpp',
])

DXBCParser_lib = static_library('DXBCParser', DXBCParser_src,
  include_directories : [ dxmt_include_path ],
)

DXBCParser_dep = declare_dependency(
  link_with           : [ DXBCParser_lib ],
  include_directories : [ dxmt_include_path ],
)

DXBCParser_native_lib = static_library('DXBCParserNative', DXBCParser_src,
  include_directories : [ ],
  native              : true,
)

DXBCParser_native_dep = declare_dependency(
  link_with           : [ DXBCParser_native_lib ],
  include_directories : [ ],
)