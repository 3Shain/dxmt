/*
 * Derived from DXVK (originally under zlib License)
 *
 * See
 * <https://github.com/doitsujin/dxvk/blob/8c4fd2723c031b425fb857454e9e5fa4ca25f4ac/src/util/util_hotpatch.h>
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#pragma once

#if defined(DECLSPEC_CHPE_PATCHABLE)
#define DXMT_HOTPATCHABLE DECLSPEC_CHPE_PATCHABLE
#elif defined(__arm64ec__)
#define DXMT_HOTPATCHABLE __attribute__((hybrid_patchable))
#else
#define DXMT_HOTPATCHABLE
#endif