dxmt_src = [
  'dxmt_format.cpp',
  'dxmt_names.cpp',
  'dxmt_buffer_pool.cpp',
  'dxmt_command_queue.cpp',
]

dxmt_lib = static_library('dxmt', dxmt_src,
  dependencies        : [ util_dep ],
  include_directories : [ dxmt_include_path ],
)

dxmt_dep = declare_dependency(
  link_with           : [ dxmt_lib ],
  include_directories : [ dxmt_include_path, include_directories('.') ],
)
