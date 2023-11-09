hlslcc_src = files([
  'cbstring/bsafe.c',
  'cbstring/bstraux.c',
  'cbstring/bstrlib.c',
  'control_flow_graph_utils.cpp',
  'control_flow_graph.cpp',
  'data_type_analysis.cpp',
  'decode.cpp',
  'hlslcc_context.cpp',
  'hlslcc_toolkit.cpp',
  'hlslcc.cpp',
  'instruction.cpp',
  'loop_transform.cpp',
  'metal_declaration.cpp',
  'metal_instruction.cpp',
  'metal_operand.cpp',
  'metal.cpp',
  'operand.cpp',
  'reflect.cpp',
  'shader_info.cpp',
  'shader.cpp',
  'use_define_chains.cpp'
])

hlslcc_lib = static_library('hlslcc', hlslcc_src,
  include_directories : [ dxmt_include_path ],
)

hlslcc_dep = declare_dependency(
  link_with           : [ hlslcc_lib ],
  include_directories : [ dxmt_include_path ],
)
