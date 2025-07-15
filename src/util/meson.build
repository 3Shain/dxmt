util_src = files([
  'util_env.cpp',
  'util_string.cpp',
  # 'util_fps_limiter.cpp',
  # 'util_flush.cpp',
  # 'util_gdi.cpp',
  # 'util_luid.cpp',
  # 'util_matrix.cpp',
  # 'util_shared_res.cpp',
  # 'util_sleep.cpp',
  'util_bloom.cpp',

  'thread.cpp',

  'com/com_guid.cpp',
  'com/com_private_data.cpp',

  'config/config.cpp',

  'log/log.cpp',

  'sha1/sha1.c',
  'sha1/sha1_util.cpp',
])

wsi_deps = []

if dxmt_native
  util_src += files([
    'wsi_platform_darwin.cpp',
  ])

  # sdl2 wsi
  lib_sdl2 = dependency('SDL2', required: false)
  if lib_sdl2.found() and get_option('dxmt_wsi_sdl2') == true
    util_src += files([
      'wsi_monitor_sdl2.cpp',
      'wsi_window_sdl2.cpp',
      'wsi_platform_sdl2.cpp',
    ])

    wsi_deps += [ lib_sdl2.partial_dependency(compile_args: true, includes: true)]
  else # headless
    util_src += files([
      'wsi_monitor_headless.cpp',
      'wsi_window_headless.cpp',
    ])
  endif
else
  util_src += files([
    'wsi_monitor_win32.cpp',
    'wsi_window_win32.cpp',
    'wsi_platform_win32.cpp',
  ])
endif

util_lib = static_library('util', util_src,
  dependencies: wsi_deps,
  include_directories : [ dxmt_include_path ],
)

util_dep = declare_dependency(
  link_with           : [ util_lib ],
  include_directories : [ include_directories('.') ],
)
