#pragma once

#include "minwindef.h"


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