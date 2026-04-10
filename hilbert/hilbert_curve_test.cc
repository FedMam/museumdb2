// TODO(FedMam): rewrite into Google Test

#include "hilbert/hilbert_curve.h"
#include "hilbert/geometry_test_util.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>

namespace ROCKSDB_NAMESPACE {
namespace {

void TEST_1ByteFull() {
  std::vector<std::vector<VarLenNumber>> encoded(0x100, std::vector<VarLenNumber>(0x100));
  for (uint32_t i = 0x00; i <= 0xff; ++i) {
    for (uint32_t j = 0x00; j <= 0xff; ++j) {
      encoded[i][j] = PointToHilbertValue(VarLenPoint2D(VarLenNumber(1, i), VarLenNumber(1, j)));

      VarLenPoint2D decoded = HilbertValueToPoint(encoded[i][j]);
      assert(decoded.GetX() == VarLenNumber(1, i));
      assert(decoded.GetY() == VarLenNumber(1, j));

      for (uint32_t i1 = 0x00; i1 <= i; ++i1) {
        for (uint32_t j1 = 0x00; (i1 < i && j1 <= 0xff) || (i1 == i && j1 < j); ++j1) {
          assert(encoded[i][j] != encoded[i1][j1]);
        }
      }
    }
  }
}

void TEST_2BytesPartial() {
  VarLenPoint2D prevp(VarLenNumber(0), VarLenNumber(0));
  VarLenNumber one(1, 0x1u);
  for (int64_t code = 0x00000000; code <= 0x00ffffff; ++code) {
      VarLenNumber encoded(4, (uint32_t)code);

      VarLenPoint2D decoded = HilbertValueToPoint(encoded);
      assert(PointToHilbertValue(decoded) == encoded);

      if (code != 0) {
        // assertion that the point is adjacent to the previous point
        assert((prevp.GetX() == decoded.GetX() && (prevp.GetY().Add(one)) == decoded.GetY()) ||
               (prevp.GetX() == decoded.GetX() && (decoded.GetY().Add(one)) == prevp.GetY()) ||
               (prevp.GetY() == decoded.GetY() && (prevp.GetX().Add(one)) == decoded.GetX()) ||
               (prevp.GetY() == decoded.GetY() && (decoded.GetX().Add(one)) == prevp.GetX()));
      }
      prevp = decoded;
  }
}


void TestNbytesRandomHelper(int max_samples, int n_bytes, int seed = 42) {
  std::mt19937 mt(seed);

  std::vector<VarLenNumber> encoded(max_samples);
  for (int i = 0; i < max_samples; ++i) {
    VarLenPoint2D point(RandomNumber(mt, n_bytes), RandomNumber(mt, n_bytes));
    encoded[i] = PointToHilbertValue(point);
    VarLenPoint2D decoded = HilbertValueToPoint(encoded[i]);
    assert(decoded.GetX() == point.GetX());
    assert(decoded.GetY() == point.GetY());

    for (int j = 0; j < i; ++j)
      assert(encoded[i] != encoded[j]);
  }
}

void TEST_2BytesRandom() {
  TestNbytesRandomHelper(10000, 2, 42);
}

void TEST_4BytesRandom() {
  TestNbytesRandomHelper(10000, 4, 43);
}

void TEST_8BytesRandom() {
  TestNbytesRandomHelper(5000, 8, 44);
}

void TEST_19BytesRandom() {
  TestNbytesRandomHelper(500, 19, 45);
}

void TEST_2BytesClose() {
  uint32_t MAX = 0xffffu;
  std::mt19937 mt(46);

  for (int dist_bit = 0; dist_bit < 16; ++dist_bit) {
    uint32_t dist = ((uint32_t)1) << dist_bit;
    for (int i = 0; i < 50000; ++i) {
      uint32_t num1 = mt() & 0xffffu;
      if (num1 > (MAX - dist)) {
        num1 &= (MAX - dist);
      }
      uint32_t num2 = num1 + dist;

      VarLenNumber code1(2, num1);
      VarLenNumber code2(2, num2);

      VarLenPoint2D point1 = HilbertValueToPoint(code1);
      VarLenPoint2D point2 = HilbertValueToPoint(code2);

      uint32_t dx = std::max(point1.GetX().NumericalValue32(), point2.GetX().NumericalValue32()) - std::min(point1.GetX().NumericalValue32(), point2.GetX().NumericalValue32());
      uint32_t dy = std::max(point1.GetY().NumericalValue32(), point2.GetY().NumericalValue32()) - std::min(point1.GetY().NumericalValue32(), point2.GetY().NumericalValue32());

      assert(dx <= dist);
      assert(dy <= dist);
    }
  }
}

// Test that if two points are close on Hilbert curve, then
// they are also close in 2D space.
void TEST_8BytesClose() {
  uint64_t MAX = 0xffffffffffffffffull;
  std::mt19937_64 mt(47);

  for (int dist_bit = 0; dist_bit < 64; ++dist_bit) {
    uint64_t dist = ((uint64_t)1) << dist_bit;
    for (int i = 0; i < 5000; ++i) {
      uint64_t num1 = mt();
      if (num1 > (MAX - dist)) {
        num1 &= (MAX - dist);
      }
      uint64_t num2 = num1 + dist;

      VarLenNumber code1(8, num1);
      VarLenNumber code2(8, num2);

      VarLenPoint2D point1 = HilbertValueToPoint(code1);
      VarLenPoint2D point2 = HilbertValueToPoint(code2);

      uint64_t dx = std::max(point1.GetX().NumericalValue64(), point2.GetX().NumericalValue64()) - std::min(point1.GetX().NumericalValue64(), point2.GetX().NumericalValue64());
      uint64_t dy = std::max(point1.GetY().NumericalValue64(), point2.GetY().NumericalValue64()) - std::min(point1.GetY().NumericalValue64(), point2.GetY().NumericalValue64());

      assert(dx <= dist);
      assert(dy <= dist);
    }
  }
}

void TEST_PointWithHilbertValue() {
  std::mt19937 mt(48);

  for (int i = 0; i < 50000; ++i) {
    VarLenNumber code = RandomNumber(mt, 4);
    VarLenPoint2D point = HilbertValueToPoint(code);

    VarLenPoint2DWithHilbertValue hpoint1(code), hpoint2(point);

    assert(hpoint1.GetNumberLength() == hpoint2.GetNumberLength());
    assert(hpoint1.GetHilbertValue() == hpoint2.GetHilbertValue());
    assert(hpoint1.GetPoint() == hpoint2.GetPoint());
  }
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(1ByteFull);
  TEST(2BytesPartial);
  TEST(2BytesRandom);
  TEST(4BytesRandom);
  TEST(8BytesRandom);
  TEST(19BytesRandom);
  TEST(2BytesClose);
  TEST(8BytesClose);
  TEST(PointWithHilbertValue);
  return 0;
}
