#include "dxbc_utils.h"
namespace dxmt {

//------------------------------------------------------------------------------
//
//  CMask methods.
//
CMask::CMask() : m_Mask(0) {}

CMask::CMask(BYTE Mask) : m_Mask(Mask) {
  DXASSERT(Mask <= DXBC::kAllCompMask, "otherwise the caller did not check");
}

CMask::CMask(BYTE c0, BYTE c1, BYTE c2, BYTE c3) {
  DXASSERT(c0 <= 1 && c1 <= 1 && c2 <= 1 && c3 <= 1,
           "otherwise the caller did not check");
  m_Mask = c0 | (c1 << 1) | (c2 << 2) | (c3 << 3);
}

CMask::CMask(BYTE StartComp, BYTE NumComp) {
  DXASSERT(StartComp < DXBC::kAllCompMask && NumComp <= DXBC::kAllCompMask &&
               (StartComp + NumComp - 1) < DXBC::kAllCompMask,
           "otherwise the caller did not check");
  m_Mask = 0;
  for (BYTE c = StartComp; c < StartComp + NumComp; c++) {
    m_Mask |= (1 << c);
  }
}

BYTE CMask::ToByte() const {
  DXASSERT(m_Mask <= DXBC::kAllCompMask, "otherwise the caller did not check");
  return m_Mask;
}

bool CMask::IsSet(BYTE c) const {
  DXASSERT(c < DXBC::kWidth, "otherwise the caller did not check");
  return (m_Mask & (1 << c)) != 0;
}

void CMask::Set(BYTE c) {
  DXASSERT(c < DXBC::kWidth, "otherwise the caller did not check");
  m_Mask = m_Mask | (1 << c);
}

CMask CMask::operator|(const CMask &o) { return CMask(m_Mask | o.m_Mask); }

BYTE CMask::GetNumActiveComps() const {
  DXASSERT(m_Mask <= DXBC::kAllCompMask, "otherwise the caller did not check");
  BYTE n = 0;
  for (BYTE c = 0; c < DXBC::kWidth; c++) {
    n += (m_Mask >> c) & 1;
  }
  return n;
}

BYTE CMask::GetNumActiveRangeComps() const {
  DXASSERT(m_Mask <= DXBC::kAllCompMask, "otherwise the caller did not check");
  if ((m_Mask & DXBC::kAllCompMask) == 0)
    return 0;

  BYTE FirstComp = 0;
  for (BYTE c = 0; c < DXBC::kWidth; c++) {
    if (m_Mask & (1 << c)) {
      FirstComp = c;
      break;
    }
  }
  BYTE LastComp = 0;
  for (BYTE c1 = 0; c1 < DXBC::kWidth; c1++) {
    BYTE c = DXBC::kWidth - 1 - c1;
    if (m_Mask & (1 << c)) {
      LastComp = c;
      break;
    }
  }

  return LastComp - FirstComp + 1;
}

BYTE CMask::MakeMask(BYTE c0, BYTE c1, BYTE c2, BYTE c3) {
  return CMask(c0, c1, c2, c3).ToByte();
}

CMask CMask::MakeXYZWMask() { return CMask(DXBC::kAllCompMask); }

CMask CMask::MakeFirstNCompMask(BYTE n) {
  switch (n) {
  case 0:
    return CMask(0, 0, 0, 0);
  case 1:
    return CMask(1, 0, 0, 0);
  case 2:
    return CMask(1, 1, 0, 0);
  case 3:
    return CMask(1, 1, 1, 0);
  default:
    DXASSERT(
        n == 4,
        "otherwise the caller did not pass the right number of components");
    return CMask(1, 1, 1, 1);
  }
}

CMask CMask::MakeCompMask(BYTE Component) {
  DXASSERT(
      Component < DXBC::kWidth,
      "otherwise the caller should have checked that the mask is non-zero");
  return CMask((BYTE)(1 << Component));
}

CMask CMask::MakeXMask() { return MakeCompMask(0); }

bool CMask::IsValidDoubleMask(const CMask &Mask) {
  BYTE b = Mask.ToByte();
  return b == 0xF || b == 0xC || b == 0x3;
}

CMask CMask::GetMaskForDoubleOperation(const CMask &Mask) {
  switch (Mask.GetNumActiveComps()) {
  case 0:
    return CMask(0, 0, 0, 0);
  case 1:
    return CMask(1, 1, 0, 0);
  case 2:
    return CMask(1, 1, 1, 1);
  }
  DXASSERT(false, "otherwise missed a case");
  return CMask();
}

BYTE CMask::GetFirstActiveComp() const {
  DXASSERT(
      m_Mask > 0,
      "otherwise the caller should have checked that the mask is non-zero");
  for (BYTE c = 0; c < DXBC::kWidth; c++) {
    if ((m_Mask >> c) & 1)
      return c;
  }
  return _UI8_MAX;
}

CMask CMask::FromDXBC(const unsigned DxbcMask) {
  return CMask(DxbcMask >> D3D10_SB_OPERAND_4_COMPONENT_MASK_SHIFT);
}
} // namespace dxmt