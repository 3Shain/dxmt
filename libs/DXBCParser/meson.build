dxbc_parser_src = files([
  'DXBCUtils.cpp',
  'BlobContainer.cpp',
  'ShaderBinary.cpp',
])

dxbc_parser_lib = static_library('DXBCParser', dxbc_parser_src,
  include_directories : [ dxmt_include_path ],
)

dxbc_parser_dep = declare_dependency(
  link_with           : [ dxbc_parser_lib ],
  include_directories : [ dxmt_include_path ],
)

if cpu_family == 'x86_64' or dxmt_native

dxbc_parser_native_lib = static_library('DXBCParserNative', dxbc_parser_src,
  include_directories : [ dxmt_native_include_path ],
  native              : not dxmt_native,
)

dxbc_parser_native_dep = declare_dependency(
  link_with           : [ dxbc_parser_native_lib ],
  include_directories : [ dxmt_native_include_path ],
)

endif