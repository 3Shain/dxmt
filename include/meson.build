dxmt_idl = [
  'dxmt_d3d12.idl',
]

dxmt_header_files = []
foreach idl_file : dxmt_idl
  header_file = idl_generator.process(idl_file)
  dxmt_header_files += header_file
endforeach