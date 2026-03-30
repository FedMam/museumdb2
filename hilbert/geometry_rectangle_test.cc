// TODO(FedMam): rewrite into Google Test

#include "hilbert/geometry.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <vector>

namespace ROCKSDB_NAMESPACE {
namespace {

void TEST_RectangleSmoke() {
  VarLenRectangle rect(
    VarLenNumber(2, 0x1234u),
    VarLenNumber(2, 0x5678u),
    VarLenNumber(2, 0x4321u),
    VarLenNumber(2, 0x8765u)
  );

  assert(rect.IsValid());
  assert(rect.GetLeft()   == VarLenNumber(2, 0x1234u));
  assert(rect.GetTop()    == VarLenNumber(2, 0x5678u));
  assert(rect.GetRight()  == VarLenNumber(2, 0x4321u));
  assert(rect.GetBottom() == VarLenNumber(2, 0x8765u));
}

void TEST_RectangleLengthAdjust() {
  VarLenRectangle rect(
    VarLenNumber(1, 0x12u),
    VarLenNumber(2, 0x3456u),
    VarLenNumber(3, 0x789abcu),
    VarLenNumber(4, 0xdef01234)
  );

  assert(rect.IsValid());
  assert(rect.GetLeft().GetLength() == 4);
  assert(rect.GetTop().GetLength() == 4);
  assert(rect.GetRight().GetLength() == 4);
  assert(rect.GetBottom().GetLength() == 4);
  assert(rect.GetLeft()   == VarLenNumber(4, 0x00000012u));
  assert(rect.GetTop()    == VarLenNumber(4, 0x00003456u));
  assert(rect.GetRight()  == VarLenNumber(4, 0x00789abcu));
  assert(rect.GetBottom() == VarLenNumber(4, 0xdef01234u));
}

void TEST_InvalidRectangle() {
  VarLenRectangle bad(
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x34u),
    VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0x33u)
  );
  assert(!bad.IsValid());

  VarLenRectangle bad1(
    VarLenNumber(1000, std::string(1000, '5')),
    VarLenNumber(1000, std::string(1000, '5')),
    VarLenNumber(1000, std::string(500, '5') + '4' + std::string(499, '5')),
    VarLenNumber(1000, std::string(500, '5') + '4' + std::string(499, '5'))
  );
  assert(!bad1.IsValid());

  VarLenRectangle bad2(
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x34u),
    VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0x35u)
  );
  assert(!bad2.IsValid());

  VarLenRectangle bad3(
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x34u),
    VarLenNumber(1, 0x13u),
    VarLenNumber(1, 0x33u)
  );
  assert(!bad3.IsValid());
}

void TEST_DegenerateRectangle() {
  VarLenRectangle degen1(
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x34u),
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x78u)
  ), degen2(
    VarLenNumber(1, 0x12u),
    VarLenNumber(1, 0x34u),
    VarLenNumber(1, 0x56u),
    VarLenNumber(1, 0x34u)
  ), degen3(
    VarLenNumber(1000, std::string(1000, '5')),
    VarLenNumber(1000, std::string(1000, '5')),
    VarLenNumber(1000, std::string(1000, '5')),
    VarLenNumber(1000, std::string(1000, '5'))
  );
  assert(degen1.IsValid() && degen2.IsValid() && degen3.IsValid());
}

void TEST_Contains() {
  VarLenRectangle rect(
    VarLenNumber(2, 0x1234u),
    VarLenNumber(2, 0x5678u),
    VarLenNumber(2, 0x9abcu),
    VarLenNumber(2, 0xdef0u)
  );
  assert(rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x1300u), VarLenNumber(2, 0x5700u))));
  assert(rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x1234u), VarLenNumber(2, 0x5700u))));
  assert(rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x1300u), VarLenNumber(2, 0x5678u))));
  assert(rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x9abcu), VarLenNumber(2, 0x5700u))));
  assert(rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x1300u), VarLenNumber(2, 0xdef0u))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x0800u), VarLenNumber(2, 0x5700u))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x1300u), VarLenNumber(2, 0xdf00u))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x0001u), VarLenNumber(2, 0x0001u))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0x0001u), VarLenNumber(2, 0xfffeu))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0xfffeu), VarLenNumber(2, 0x0001u))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(2, 0xfffeu), VarLenNumber(2, 0xfffeu))));
  assert(!rect.Contains(VarLenPoint2D(VarLenNumber(4, 0xfffffffeu), VarLenNumber(4, 0xfffffffeu))));
}

void TEST_Intersects1() {
  VarLenRectangle rectA(
    VarLenNumber(2, 0x1000u),
    VarLenNumber(2, 0x1000u),
    VarLenNumber(2, 0x3000u),
    VarLenNumber(2, 0x3000u)
  ), rectB(
    VarLenNumber(2, 0x4000u),
    VarLenNumber(2, 0x1000u),
    VarLenNumber(2, 0x6000u),
    VarLenNumber(2, 0x3000u)
  ), rectC(
    VarLenNumber(2, 0x1000u),
    VarLenNumber(2, 0x4000u),
    VarLenNumber(2, 0x3000u),
    VarLenNumber(2, 0x6000u)
  ), rectD(
    VarLenNumber(2, 0x4000u),
    VarLenNumber(2, 0x4000u),
    VarLenNumber(2, 0x6000u),
    VarLenNumber(2, 0x6000u)
  ), rectE(
    VarLenNumber(2, 0x2000u),
    VarLenNumber(2, 0x2000u),
    VarLenNumber(2, 0x5000u),
    VarLenNumber(2, 0x5000u)
  );

  assert(rectA.Intersects(rectE) && rectE.Intersects(rectA));
  assert(rectB.Intersects(rectE) && rectE.Intersects(rectB));
  assert(rectC.Intersects(rectE) && rectE.Intersects(rectC));
  assert(rectD.Intersects(rectE) && rectE.Intersects(rectD));
  assert(!rectA.Intersects(rectB) && !rectB.Intersects(rectA));
  assert(!rectA.Intersects(rectC) && !rectC.Intersects(rectA));
  assert(!rectA.Intersects(rectD) && !rectD.Intersects(rectA));
  assert(!rectB.Intersects(rectC) && !rectC.Intersects(rectB));
  assert(!rectB.Intersects(rectD) && !rectD.Intersects(rectB));
  assert(!rectC.Intersects(rectD) && !rectD.Intersects(rectC));
}

void TEST_Intersects2() {
  std::vector<VarLenRectangle> rectHierarchy = {
      VarLenRectangle(
      VarLenNumber(1, 0x10u),
      VarLenNumber(1, 0x10u),
      VarLenNumber(1, 0x90u),
      VarLenNumber(1, 0x90u)
    ), VarLenRectangle(
      VarLenNumber(1, 0x10u),
      VarLenNumber(1, 0x20u),
      VarLenNumber(1, 0x80u),
      VarLenNumber(1, 0x80u)
    ), VarLenRectangle(
      VarLenNumber(1, 0x20u),
      VarLenNumber(1, 0x20u),
      VarLenNumber(1, 0x70u),
      VarLenNumber(1, 0x70u)
    ), VarLenRectangle(
      VarLenNumber(1, 0x30u),
      VarLenNumber(1, 0x30u),
      VarLenNumber(1, 0x70u),
      VarLenNumber(1, 0x60u)
    ), VarLenRectangle(
      VarLenNumber(1, 0x40u),
      VarLenNumber(1, 0x40u),
      VarLenNumber(1, 0x60u),
      VarLenNumber(1, 0x60u)
    ), VarLenRectangle(
      VarLenNumber(1, 0x48u),
      VarLenNumber(1, 0x48u),
      VarLenNumber(1, 0x58u),
      VarLenNumber(1, 0x58u)
    )
  };

  for (auto& rect1: rectHierarchy)
    for (auto& rect2: rectHierarchy)
      assert(rect1.Intersects(rect2));
}

void TEST_Intersects3() {
  std::vector<VarLenRectangle> rectGrid;
  for (uint32_t i = 0; i < 3; ++i)
    for (uint32_t j = 0; j < 3; ++j)
      rectGrid.push_back(VarLenRectangle(
        VarLenNumber(1, 0x10u * i),
        VarLenNumber(1, 0x10u * j),
        VarLenNumber(1, 0x10u * (i+1)),
        VarLenNumber(1, 0x10u * (j+1))
      ));
  
  for (int32_t i1 = 0; i1 < 3; ++i1)
    for (int32_t j1 = 0; j1 < 3; ++j1)
      for (int32_t i2 = 0; i2 < 3; ++i2)
        for (int32_t j2 = 0; j2 < 3; ++j2)
          assert(rectGrid[j1 + 3 * i1].Intersects(rectGrid[j2 + 3 * i2])
                  == (std::abs(i1 - i2) <= 1 && std::abs(j1 - j2) <= 1));
}

void TEST_MBR() {
  std::vector<VarLenRectangle> rectGrid;
  for (uint32_t i = 0; i < 3; ++i)
    for (uint32_t j = 0; j < 3; ++j)
      rectGrid.push_back(VarLenRectangle(
        VarLenNumber(1, 0x10u * i),
        VarLenNumber(1, 0x10u * j),
        VarLenNumber(1, 0x10u * (i+1)),
        VarLenNumber(1, 0x10u * (j+1))
      ));
  
  for (int32_t i1 = 0; i1 < 3; ++i1)
    for (int32_t j1 = 0; j1 < 3; ++j1)
      for (int32_t i2 = 0; i2 < 3; ++i2)
        for (int32_t j2 = 0; j2 < 3; ++j2) {
          auto mbr = rectGrid[j1 + 3 * i1].MBR(rectGrid[j2 + 3 * i2]);
          assert(mbr.GetLeft()   == VarLenNumber(1, 0x10u * (uint32_t)std::min(i1, i2)));
          assert(mbr.GetTop()    == VarLenNumber(1, 0x10u * (uint32_t)std::min(j1, j2)));
          assert(mbr.GetRight()  == VarLenNumber(1, 0x10u * (uint32_t)(std::max(i1, i2)+1)));
          assert(mbr.GetBottom() == VarLenNumber(1, 0x10u * (uint32_t)(std::max(j1, j2)+1)));
        }
  
  VarLenRectangle big(
    VarLenNumber(1, 0x01u),
    VarLenNumber(1, 0x01u),
    VarLenNumber(1, 0xfeu),
    VarLenNumber(1, 0xfeu)
  ), small(
    VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0xeeu),
    VarLenNumber(1, 0xeeu)
  ), mbr1 = big.MBR(small);
  assert(mbr1.GetLeft() == big.GetLeft() &&
          mbr1.GetTop() == big.GetTop() &&
          mbr1.GetRight() == big.GetRight() &&
          mbr1.GetBottom() == big.GetBottom());
}

void TEST_MBRDifferentLength() {
  VarLenRectangle small(
    VarLenNumber(1, 0x01u),
    VarLenNumber(1, 0x01u),
    VarLenNumber(1, 0xfeu),
    VarLenNumber(1, 0xfeu)
  ), big(
    VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0x11u),
    VarLenNumber(2, 0xeeeeu),
    VarLenNumber(2, 0xeeeeu)
  ), mbr = small.MBR(big);
  assert(mbr.GetLeft() == small.GetLeft() &&
          mbr.GetTop() == small.GetTop() &&
          mbr.GetRight() == big.GetRight() &&
          mbr.GetBottom() == big.GetBottom());
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(RectangleSmoke);
  TEST(RectangleLengthAdjust);
  TEST(InvalidRectangle);
  TEST(DegenerateRectangle);
  TEST(Contains);
  TEST(Intersects1);
  TEST(Intersects2);
  TEST(Intersects3);
  TEST(MBR);
  TEST(MBRDifferentLength);

  return 0;
}
