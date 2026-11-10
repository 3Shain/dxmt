#include "util_shader_dump.hpp"
#include "util_env.hpp"
#include "log/log.hpp"
#include <cstdio>
#include <string>

namespace dxmt {

/*
 * DXMT_SHADER_DUMP=<dir>: write every unique shader bytecode received from
 * the application to <dir>/<sha1>.dxbc
 */
void
DumpShaderBytecode(const Sha1Digest &sha1, const void *pBytecode, uint32_t BytecodeLength) {
  static const std::string dump_dir = [] {
    auto dir = env::getEnvVar("DXMT_SHADER_DUMP");
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
      dir += '/';
    return dir;
  }();
  if (dump_dir.empty())
    return;
  auto path = dump_dir + sha1.string() + ".dxbc";
  if (FILE *f = std::fopen(path.c_str(), "wb")) {
    std::fwrite(pBytecode, 1, BytecodeLength, f);
    std::fclose(f);
  } else {
    WARN("DXMT_SHADER_DUMP: failed to write ", path);
  }
}

} // namespace dxmt
