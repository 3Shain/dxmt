
lib_d3dcompiler = cpp.find_library('d3dcompiler_47')

executable('generate_kernel', ['generate_kernel.cpp'],
  dependencies        : [ winemetal_dep, airconv_dep, lib_d3dcompiler ],
)