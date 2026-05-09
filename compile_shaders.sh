#!/bin/bash
echo "Pre-compiling shaders from /tmp/dxmt_shader_cache/..."
cd /tmp/dxmt_shader_cache
for f in *.dxbc; do
  [ -f "$f" ] || continue
  base="${f%.dxbc}"
  if [ ! -f "${base}.metallib" ]; then
    /usr/local/bin/metal-shaderconverter -o "${base}.metallib" "$f" --output-reflection-file="${base}.json" 2>/dev/null
    echo "  Compiled $f -> ${base}.metallib"
  else
    echo "  Already cached: ${base}.metallib"
  fi
done
echo "Done. Shaders ready for RE4."
