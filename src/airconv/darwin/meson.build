
use_local_native_llvm = get_option('local_native_llvm')

if use_local_native_llvm
# At project root:
# 
# mkdir -p ./toolchains/llvm-darwin-build
# cmake -B ./toolchains/llvm-darwin-build -S ./toolchains/llvm-project/llvm-darwin \
#   -DCMAKE_INSTALL_PREFIX="$(pwd)/toolchains/llvm-darwin" \
#   -DLLVM_ENABLE_ASSERTIONS=On \
#   -DCMAKE_BUILD_TYPE=Release \
#   -DBUG_REPORT_URL="https://github.com/3Shain/dxmt" \
#   -DPACKAGE_VENDOR="DXMT" \
#   -DLLVM_VERSION_PRINTER_SHOW_HOST_TARGET_INFO=Off \
#   -G Ninja
# pushd ./toolchains/llvm-darwin-build
# ninja
# ninja install
# popd

llvm_include_path_darwin = include_directories('../../../toolchains/llvm-darwin/include') # FIXME: in favor of path relative to project
llvm_ld_flags_darwin = [
 '-L'+join_paths(meson.source_root(), 'toolchains/llvm-darwin/lib'),
 '-lm','-lz', '-lzstd', '-lcurses', '-lxml2'
]
else
# `brew install llvm@15`
llvm_include_path_darwin = include_directories('/usr/local/opt/llvm@15/include') # FIXME: in favor of path relative to project
llvm_ld_flags_darwin = [
 '-L'+join_paths('/usr/local/opt/llvm@15/lib'),
 '-lm','-lz', '-lzstd', '-lcurses', '-lxml2'
]
endif

airconv_lib_darwin = static_library('airconv', airconv_src,
  include_directories : [ dxmt_include_path, llvm_include_path_darwin ],
  cpp_args            : llvm_cxx_flags,
  dependencies        : [ DXBCParser_native_dep ],
  link_args           : llvm_ld_flags_darwin,
  native              : true,
)

executable('airconv', airconv_src + airconv_cli_src,
  include_directories : [ dxmt_include_path, llvm_include_path_darwin ],
  cpp_args            : llvm_cxx_flags,
  dependencies        : [ DXBCParser_native_dep ],
  link_args           : [ llvm_ld_flags_darwin, llvm_deps ],
  native              : true
)