#pragma once
#include "d3dcommon.h"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include <cstdio>
#include <cassert>

#define DXASSERT_NOMSG assert

#define DXASSERT_LOCALVAR(local, exp, msg) DXASSERT(exp, msg)

#define DXASSERT_LOCALVAR_NOMSG(local, exp) DXASSERT_LOCALVAR(local, exp, "")

#define DXVERIFY_NOMSG assert

#define DXASSERT_ARGS(expr, fmt, ...)                                          \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, fmt, __VA_ARGS__);                                       \
      assert(false);                                                           \
    }                                                                          \
  } while (0);

#define DXASSERT(expr, msg)                                                    \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, msg);                                                    \
      assert(false && msg);                                                    \
    }                                                                          \
  } while (0);

#define DXASSERT_DXBC(__exp)                                                   \
  DXASSERT(__exp, "otherwise incorrect assumption about DXBC")
  
namespace DXBC {

// Width of DXBC vector operand.
const BYTE kWidth = 4;
// DXBC mask with all active components.
const BYTE kAllCompMask = 0x0F;
} // namespace DXBC
namespace dxmt {
/// Use this class to represent DXBC register component mask.
class CMask {
public:
  CMask();
  CMask(BYTE Mask);
  CMask(BYTE c0, BYTE c1, BYTE c2, BYTE c3);
  CMask(BYTE StartComp, BYTE NumComp);

  BYTE ToByte() const;

  static bool IsSet(BYTE Mask, BYTE c);
  bool IsSet(BYTE c) const;
  void Set(BYTE c);

  CMask operator|(const CMask &o);

  BYTE GetNumActiveComps() const;
  BYTE GetNumActiveRangeComps() const;
  bool IsZero() const { return GetNumActiveComps() == 0; }

  BYTE GetFirstActiveComp() const;

  static BYTE MakeMask(BYTE c0, BYTE c1, BYTE c2, BYTE c3);
  static CMask MakeXYZWMask();
  static CMask MakeFirstNCompMask(BYTE n);
  static CMask MakeCompMask(BYTE Component);
  static CMask MakeXMask();

  static bool IsValidDoubleMask(const CMask &Mask);
  static CMask GetMaskForDoubleOperation(const CMask &Mask);

  static CMask FromDXBC(const unsigned DxbcMask);

protected:
  BYTE m_Mask;
};
} // namespace dxmt