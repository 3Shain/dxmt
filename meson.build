project('dxmt', ['c', 'cpp'], version : 'v0.1', default_options : [ 
  'cpp_std=c++20', 
  'warning_level=2', 
  'build.cpp_std=c++20',
  'build.warning_level=2',
  'b_ndebug=if-release',
])

cpp = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
clang = meson.get_compiler('c', native: true)
clangpp = meson.get_compiler('cpp', native: true)

dxmt_include_dirs = [
  './include',
  './libs'
]

compiler_args = [
  '-msse',
  '-msse2',
  '-msse3',
  '-mfpmath=sse',
  '-Wimplicit-fallthrough',
  # gcc
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wno-cast-function-type', # Needed for GetProcAddress.
  # clang
  '-Wno-unused-private-field',
  '-Wno-microsoft-exception-spec',
  '-Wno-extern-c-compat',
  '-Wno-unused-const-variable',
  '-Wno-missing-braces',
  '-fblocks',
]

native_compiler_args = [
  '-Wimplicit-fallthrough',
  '-Wno-unused-parameter',
  '-Wno-unused-private-field',
  '-Wno-unused-const-variable',
  '-Wno-missing-braces',
  '-fblocks',
]

link_args = []

compiler_args += [
  '-DNOMINMAX',
  '-D_WIN32_WINNT=0xa00',
]

link_args += [
  '-static',
  # -static is sufficient for clang, while c runtime will be linked to api-ms-win-crt-* (ucrt)
  # '-static-libgcc',
  # '-static-libstdc++',
  # We need to set the section alignment for debug symbols to
  # work properly as well as avoiding a memcpy from the Wine loader.
  '-Wl,--file-alignment=4096',
]

# Wine's built-in back traces only work with dwarf4 symbols
if get_option('debug')
  compiler_args += [
    '-gdwarf-4',
  ]
endif

dxmt_include_path = include_directories(dxmt_include_dirs)

res_ext = '.o'
wrc = find_program('windres')
wrc_generator = generator(wrc,
  output    : [ '@BASENAME@' + res_ext ],
  arguments : [ '-i', '@INPUT@', '-o', '@OUTPUT@' ],
)

add_project_arguments(cpp.get_supported_arguments(compiler_args), language: 'cpp')
add_project_arguments(clangpp.get_supported_arguments(native_compiler_args), language: 'cpp', native: true)
add_project_arguments(cc.get_supported_arguments(compiler_args), language: 'c')
add_project_link_arguments(cpp.get_supported_link_arguments(link_args), language: 'cpp')
add_project_link_arguments(cc.get_supported_link_arguments(link_args), language: 'c')
add_project_arguments(['-arch', 'x86_64'], language: 'c',  native: true)
add_project_arguments(['-arch', 'x86_64'], language: 'cpp',  native: true)
add_project_link_arguments(['-arch', 'x86_64'], language: 'cpp',  native: true)

dxmt_debug = get_option('dxmt_debug')

if dxmt_debug
add_project_arguments('-DDXMT_DEBUG=1', language: 'cpp')
endif

dxmt_version = vcs_tag(
  command: ['git', 'describe', '--always'],
  input:  'version.h.in',
  output: 'version.h',
)

subdir('libs')
subdir('src')
subdir('tests')