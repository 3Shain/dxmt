dxmt_src = [
  'dxmt_command_stream.cpp',
  'dxmt_pipeline.cpp',
  'dxmt_format.cpp',
]

dxmt_extra_deps = [ ]

dxmt_lib = static_library('dxmt', dxmt_src,
  link_with           : [ util_lib ],
  dependencies        : dxmt_extra_deps,
  include_directories : [ dxmt_include_path ],
)

dxmt_dep = declare_dependency(
  link_with           : [ dxmt_lib ],
  include_directories : [ dxmt_include_path ],
)
