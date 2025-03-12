#!/bin/sh

UNIX_INSTALL_DIR=$2

mkdir -p "${MESON_INSTALL_PREFIX}/${UNIX_INSTALL_DIR}"
cp "${MESON_BUILD_ROOT}/src/winemetal/unix/winemetal.so" "${MESON_INSTALL_PREFIX}/${UNIX_INSTALL_DIR}/winemetal.so"


if [ $1 == "true" ]; then
    strip "${MESON_INSTALL_PREFIX}/${UNIX_INSTALL_DIR}/winemetal.so" -x
fi
