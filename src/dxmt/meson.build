dxmt_src = [
  'dxmt_format.cpp',
  'dxmt_names.cpp',
  'dxmt_buffer_pool.cpp',
  'dxmt_command_queue.cpp',
  'dxmt_command.cpp',
  'dxmt_capture.cpp',
  'dxmt_counter_pool.cpp',
]

air = custom_target('metal_air',
  input: 'dxmt_command.metal',
  output: 'metal_air',
  command: [
    find_program('xcrun'), '-sdk', 'macosx', 'metal',
    '-o', '@OUTPUT@', '-c', '@INPUT@'
  ]
)

metal_lib = custom_target('metal_lib',
  output: 'metal_lib',
  input: air,
  command: [
    find_program('xcrun'), '-sdk', 'macosx', 'metallib',
    '-o', '@OUTPUT@', '@INPUT@'
  ]
)

metal_lib_bin = custom_target('objectify-data',
  output : 'command_bin.c',
  input : metal_lib,
  command : [ 
    find_program('xxd'),
    '-n', 'dxmt_command_metallib',
    '-i', '@INPUT@',
    '@OUTPUT@', 
  ]
)

dxmt_lib = static_library('dxmt', dxmt_src, metal_lib_bin,
  dependencies        : [ util_dep ],
  include_directories : [ dxmt_include_path ],
)

dxmt_dep = declare_dependency(
  link_with           : [ dxmt_lib ],
  include_directories : [ dxmt_include_path, include_directories('.') ],
)
