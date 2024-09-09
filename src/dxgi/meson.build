dxgi_res = wrc_generator.process('version.rc')

dxgi_src = [
  'dxgi_adapter.cpp',
  'dxgi_factory.cpp',
  'dxgi_output.cpp',
  'dxgi_options.cpp',
  'dxgi.cpp',
]

dxgi_ld_args      = []
dxgi_link_depends = []

dxgi_dll = shared_library('dxgi', dxgi_src, dxgi_res,
  name_prefix         : '',
  dependencies        : [ dxmt_dep, util_dep, winemetal_dep ],
  include_directories : [ dxmt_include_path ],
  install             : true,
  vs_module_defs      : 'dxgi.def',
  link_args           : dxgi_ld_args,
  link_depends        : [ dxgi_link_depends ],
)

dxgi_dep = declare_dependency(
  link_with           : [ dxgi_dll ],
  include_directories : [ dxmt_include_path, include_directories('.') ],
)
