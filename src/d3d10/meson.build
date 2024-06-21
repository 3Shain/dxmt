d3d10core_res = wrc_generator.process('version.rc')

d3d10core_src = [
  'd3d10core.cpp',
]

d3d10_ld_args      = []
d3d10_link_depends = []

d3d10core_dll = shared_library('d3d10core', d3d10core_src, d3d10core_res,
  name_prefix         : '',
  dependencies        : [ dxgi_dep, util_dep ],
  include_directories : [ dxmt_include_path ],
  install             : true,
  vs_module_defs      : 'd3d10core.def',
  link_args           : d3d10_ld_args,
  link_depends        : [ d3d10_link_depends ],
)

d3d10core_dep = declare_dependency(
  link_with           : [ d3d10core_dll ],
  include_directories : [ dxmt_include_path ],
)