// TODO(FedMam): rewrite into Google Test

#include "hilbert/geometry.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>

#include <iostream>

namespace ROCKSDB_NAMESPACE {
namespace {

void TEST_Default() {
  VarLenNumber x(500);
  assert(x.GetLength() == 500);
  assert(x.GetReprLittleEndian() == std::string(500, '\0'));
}

void TEST_FromString() {
  VarLenNumber x(2, "AB");
  assert(x.GetLength() == 2);
  assert(x.GetReprLittleEndian() == "AB");
  assert(x.GetReprBigEndian() == "BA");
}

void TEST_FromStringIncorrectLength() {
  VarLenNumber x(2, "ABCD");
  VarLenNumber y(6, "ABCD");
  assert(x.GetLength() == 2);
  assert(y.GetLength() == 6);

  assert(x.GetReprLittleEndian() == "AB");
  assert(y.GetReprLittleEndian() == std::string("ABCD\0\0", 6));
}

void TEST_FromInteger() {
  VarLenNumber x(4, static_cast<uint32_t>(
    'A' + ('B' << 8) + ('C' << 16) + ('D' << 24)
  ));
  VarLenNumber y(8, static_cast<uint64_t>(
    'A' + ('B' << 8) + ('C' << 16) + ('D' << 24) +
    ((uint64_t)'E' << 32) + ((uint64_t)'F' << 40) +
    ((uint64_t)'G' << 48) + ((uint64_t)'H' << 56)
  ));
  
  assert(x.GetLength() == 4);
  assert(y.GetLength() == 8);
  assert(x.GetReprLittleEndian() == "ABCD");
  assert(y.GetReprLittleEndian() == "ABCDEFGH");
  assert(x.GetReprBigEndian() == "DCBA");
  assert(y.GetReprBigEndian() == "HGFEDCBA");

  VarLenNumber z(2, static_cast<uint32_t>(
    'A' + ('B' << 8) + ('C' << 16) + ('D' << 24)
  ));
  assert(z.GetLength() == 2);
  assert(z.GetReprLittleEndian() == "AB");
  assert(z.GetReprBigEndian() == "BA");

  VarLenNumber t(4, (uint32_t)0);
  assert(t.GetLength() == 4);
  assert(t.GetReprLittleEndian() == std::string("\0\0\0\0", 4));
}

void TEST_VeryLong() {
  size_t N = 0x4000000;
  VarLenNumber x(N, std::string(N, '5'));
  assert(x.GetLength() == N);
  assert(x.GetReprLittleEndian() == std::string(N, '5'));
}

void TEST_LengthOnlyConstructor() {
  VarLenNumber x, y(2);
  assert(x.GetLength() == 0);
  assert(x.GetReprLittleEndian() == "");
  assert(y.GetLength() == 2);
  assert(y.GetReprLittleEndian() == std::string("\0\0", 2));
  assert(x == y);
}

void TEST_NumericalStringRepr() {
  VarLenNumber x(4, 0x56781234u);
  assert(x.NumericalStringRepr() == "56781234");
  VarLenNumber y(8, (uint64_t)0x56781234abcdef09);
  assert(y.NumericalStringRepr() == "56781234abcdef09");
  VarLenNumber z(10, "\n\n\n\n\n\n\n\n\n\n");
  assert(z.NumericalStringRepr() == "0a0a0a0a0a0a0a0a0a0a");
  VarLenNumber t(4, 0x02u);
  assert(t.NumericalStringRepr() == "00000002");
}

void TEST_NumericalValue() {
  VarLenNumber x, y(1, 0x12u), z(2, 0x3456u), t(4, 0x789abcdeu);
  VarLenNumber u(8, (uint64_t)0xf0e1d2c3b4a59687u);

  assert(x.NumericalValue32() == 0);
  assert(x.NumericalValue64() == 0);
  assert(y.NumericalValue32() == 0x12);
  assert(y.NumericalValue64() == 0x12);
  assert(z.NumericalValue32() == 0x3456);
  assert(z.NumericalValue64() == 0x3456);
  assert(t.NumericalValue32() == 0x789abcde);
  assert(t.NumericalValue64() == 0x789abcde);
  assert(u.NumericalValue64() == 0xf0e1d2c3b4a59687u);
}

void TEST_GetByteAt() {
  VarLenNumber x(3, "ABC");
  assert(x.GetByteAt(0) == 'A');
  assert(x.GetByteAt(1) == 'B');
  assert(x.GetByteAt(2) == 'C');
  assert(x.GetByteAt(3) == '\0');
  assert(x.GetByteAt(4) == '\0');
  assert(x.GetByteAt(5555555) == '\0');
  assert(x[0] == 'A');
  assert(x[1] == 'B');
  assert(x[2] == 'C');
  assert(x[3] == '\0');
}

void TEST_CropOrResize() {
  VarLenNumber x(4, "ABCD");
  VarLenNumber y = x.CropOrResize(2);
  VarLenNumber z = x.CropOrResize(6);
  assert(y.GetLength() == 2);
  assert(z.GetLength() == 6);
  assert(y.GetReprLittleEndian() == "AB");
  assert(z.GetReprLittleEndian() == std::string("ABCD\0\0", 6));
  VarLenNumber t = x.CropOrResize(5555555);
  assert(t.GetLength() == 5555555);
  assert(t.GetReprLittleEndian().substr(0, 6) == std::string("ABCD\0\0", 6));
}

void TEST_Compare() {
  VarLenNumber x(4, "ABCD"), x1(4, "ABCD");
  VarLenNumber y(3, "ABC"), y1(4, "ABC"), y2(3, "ABCD");
  VarLenNumber za(4, "ABCE"), zb(4, "ABDD"), zc(4, "ACCD"), zd(4, "BBCD");
  VarLenNumber ze(4, "ABCC"), zf(4, "ABBD"), zg(4, "AACD"), zh(4, "@BCD");
  VarLenNumber t(5, "ABCD ");
  VarLenNumber x2(5, "ABCD");

  assert(x == x1 && x == x2);
  assert(y == y1 && y == y2);
  assert(x > y && x1 > y1 && x2 > y2);
  assert(x < za && x < zb && x < zc && x < zd);
  assert(x > ze && x > zf && x > zg && x > zh);
  assert(x < t);
  assert(x <= x1 && x >= x1);
  assert(x <= za && x >= ze);
  assert(x != y && x != t && x != za && x != ze);
  assert(x == x && x <= x && x >= x);

  VarLenNumber p(1000, std::string(1000, '5'));
  VarLenNumber q(1000, std::string(499, '5') + '6' + std::string(500, '5'));
  VarLenNumber r(1000, std::string(499, '5') + '4' + std::string(500, '5'));
  VarLenNumber p1(2000, std::string(1000, '5') + std::string(1000, '\0'));
  assert(p < q && p <= q && p != q && p == p);
  assert(p > r && p >= r && p != r && p == p1);
}

void TEST_Add() {
  assert(VarLenNumber(4, 2u) + VarLenNumber(4, 3u) == VarLenNumber(4, 5u));
  assert(VarLenNumber(1, 2u) + VarLenNumber(1, 3u) == VarLenNumber(1, 5u));
  assert(VarLenNumber(4, 0x12345678u) + VarLenNumber(4, 0x98765432u) == VarLenNumber(4, 0xaaaaaaaau));
  assert(VarLenNumber(1, 0xfeu) + VarLenNumber(1, 0x3u) == VarLenNumber(2, 0x101u));
  assert(VarLenNumber(2, 0x8000u) + VarLenNumber(2, 0x8000u) == VarLenNumber(3, 0x10000u));
  assert(VarLenNumber(8, (uint64_t)0x1234567887654321ull) + VarLenNumber(8, (uint64_t)0x9876543223456789ull) == VarLenNumber(8, (uint64_t)0xaaaaaaaaaaaaaaaaull));
  assert(VarLenNumber(4, 0x80000000u) + VarLenNumber(4, 0x80000000u) == VarLenNumber(5, (uint64_t)0x100000000ull));

  assert(VarLenNumber(4, 0x12345678u) + VarLenNumber(1, 0x80u) == VarLenNumber(4, 0x123456f8u));
  assert(VarLenNumber(1, 0x80u) + VarLenNumber(4, 0x12345678u) == VarLenNumber(4, 0x123456f8u));

  assert(VarLenNumber(4, 2u).Add(VarLenNumber(4, 3u)) == VarLenNumber(4, 5u));
  assert(VarLenNumber(1, 2u).Add(VarLenNumber(1, 3u)) == VarLenNumber(1, 5u));
  
  assert(VarLenNumber(1, 0xfeu).Add(VarLenNumber(1, 0x3u), false) == VarLenNumber(2, 0x101u));
  assert(VarLenNumber(2, 0x8000u).Add(VarLenNumber(2, 0x8000u), false) == VarLenNumber(3, 0x10000u));
  assert(VarLenNumber(2, 0x8001u).Add(VarLenNumber(2, 0x7fffu), false) == VarLenNumber(3, 0x10000u));

  assert(VarLenNumber(2, 0x8001u).Add(VarLenNumber(2, 0x8001u), true) == VarLenNumber(2, 0x0002u));
  assert(VarLenNumber(2, 0x8000u).Add(VarLenNumber(2, 0x8000u), true) == VarLenNumber(2, 0x0000u));
  assert(VarLenNumber(2, 0x8001u).Add(VarLenNumber(2, 0x7fffu), true) == VarLenNumber(2, 0x0000u));
  assert(VarLenNumber(3, 0x80fffeu).Add(VarLenNumber(2, 0x0002u), true) == VarLenNumber(3, 0x810000u));
}

void TEST_AndOrXor() {
  VarLenNumber x(4, 0xf0c3a796u);
  VarLenNumber y(4, 0xf609c73au);
  VarLenNumber z(4, 0x23456789u);
  VarLenNumber nx(4, 0x0f3c5869u);

  assert((x & y) == VarLenNumber(4, 0xf0018712u));
  assert((x | y) == VarLenNumber(4, 0xf6cbe7beu));
  assert((x ^ y) == VarLenNumber(4, 0x06ca60acu));

  assert(x.And(z) == VarLenNumber(4, 0x20412780u));
  assert(x.Or(z)  == VarLenNumber(4, 0xf3c7e79fu));
  assert(x.Xor(z) == VarLenNumber(4, 0xd386c01fu));

  assert((x & nx) == VarLenNumber(4, 0u));
  assert((x | nx) == VarLenNumber(4, 0xffffffffu));
  assert((x ^ nx) == VarLenNumber(4, 0xffffffffu));
}

void TEST_AndOrXorAssign() {
  VarLenNumber x(4, 0xf0c3a796u);
  VarLenNumber x1 = x, x2 = x, x3 = x;
  VarLenNumber y(4, 0xf609c73au);
  VarLenNumber z(4, 0x23456789u);

  x1 &= y; x2 |= y; x3 ^= y;
  assert(x1 == VarLenNumber(4, 0xf0018712u));
  assert(x2 == VarLenNumber(4, 0xf6cbe7beu));
  assert(x3 == VarLenNumber(4, 0x06ca60acu));

  x1 = x2 = x3 = x;
  x1.AndAssign(z);
  x2.OrAssign(z);
  x3.XorAssign(z);

  assert(x1 == VarLenNumber(4, 0x20412780u));
  assert(x2 == VarLenNumber(4, 0xf3c7e79fu));
  assert(x3 == VarLenNumber(4, 0xd386c01fu));
}

void TEST_AndOrXorDifferentLength() {
  VarLenNumber x(4, 0x12345678u);
  VarLenNumber y(2, 0x3333u);
  VarLenNumber z(6, (uint64_t)0xccccccccccccull);

  assert((x & y) == VarLenNumber(4, 0x00001230u));
  assert((x | y) == VarLenNumber(4, 0x1234777bu));
  assert((x ^ y) == VarLenNumber(4, 0x1234654bu));

  assert((x & z).GetLength() == 6);
  assert((x & z) == VarLenNumber(6, (uint64_t)0x000000044448ull));
  assert((x | z) == VarLenNumber(6, (uint64_t)0xccccdefcdefcull));
  assert((x ^ z) == VarLenNumber(6, (uint64_t)0xccccdef89ab4ull));

  x.XorAssign(y);
  assert(x == VarLenNumber(4, 0x1234654bu));
}

void TEST_AllPositive() {
  for (uint32_t i = 0; i <= 32; ++i) {
    VarLenNumber x = VarLenNumber::AllPositive(i);
    for (uint32_t j = 0; j < 32; ++j) {
      if (j < i) assert(x.IsBitSetAt(j));
      else assert(!x.IsBitSetAt(j));
    }
  }
}

void TEST_RShift() {
  VarLenNumber x(2, 0x5678u);
  for (uint32_t i = 0; i <= 17; ++i) {
    assert((x >> i) == VarLenNumber(2, (uint32_t)(0x5678 >> i)));
  }

  VarLenNumber y = VarLenNumber::AllPositive(1000);
  for (uint32_t i = 0; i <= 100; ++i) {
    assert((y >> i) == VarLenNumber::AllPositive(1000 - i));
  }
}

void TEST_IsBitSetAt() {
  VarLenNumber x(2, 0x5678u);
  for (uint32_t i = 0; i <= 17; ++i) {
    assert(x.IsBitSetAt(i) == ((0x5678 & (1 << i)) != 0));
  }
  assert(!x.IsBitSetAt(5555555));

  VarLenNumber y = VarLenNumber::AllPositive(1000);
  for (uint32_t i = 0; i < 1200; ++i) {
    assert((y.IsBitSetAt(i)) == (i < 1000));
  }
}

void TEST_ModifyBitAt() {
  VarLenNumber x(2, 0x5678u);
  x.SetBitAt(8);
  assert(x == VarLenNumber(2, 0x5778u));
  x.SetBitAt(3);
  assert(x == VarLenNumber(2, 0x5778u));
  x.UnsetBitAt(14);
  assert(x == VarLenNumber(2, 0x1778u));
  x.UnsetBitAt(1);
  assert(x == VarLenNumber(2, 0x1778u));
  x.InvertBitAt(5);
  assert(x == VarLenNumber(2, 0x1758u));
  x.InvertBitAt(11);
  assert(x == VarLenNumber(2, 0x1f58u));
  x.SetBitValueAt(13, true);
  assert(x == VarLenNumber(2, 0x3f58u));
  x.SetBitValueAt(0, false);
  assert(x == VarLenNumber(2, 0x3f58u));
  x.SetBitValueAt(4, false);
  assert(x == VarLenNumber(2, 0x3f48u));
}

void TEST_InvertNLowestBits() {
  VarLenNumber x(3, 0x56789au);
  x.InvertNLowestBits(8);
  assert(x == VarLenNumber(3, 0x567865u));
  x.InvertNLowestBits(16);
  assert(x == VarLenNumber(3, 0x56879au));
  x.InvertNLowestBits(24);
  assert(x == VarLenNumber(3, 0xa97865u));
  x.InvertNLowestBits(9);
  assert(x == VarLenNumber(3, 0xa9799au));
  x.InvertNLowestBits(23);
  assert(x == VarLenNumber(3, 0xd68665u));
  x.InvertNLowestBits(12);
  assert(x == VarLenNumber(3, 0xd6899au));
  x.InvertNLowestBits(3);
  assert(x == VarLenNumber(3, 0xd6899du));
}

void TEST_CropBits() {
  VarLenNumber x(3, 0x56789au);
  assert(x.CropBits(8)  == VarLenNumber(1, 0x9au));
  assert(x.CropBits(16) == VarLenNumber(2, 0x789au));
  assert(x.CropBits(24) == VarLenNumber(3, 0x56789au));
  assert(x.CropBits(32) == VarLenNumber(4, 0x0056789au));
  assert(x.CropBits(9)  == VarLenNumber(2, 0x009au));
  assert(x.CropBits(13) == VarLenNumber(2, 0x189au));
  assert(x.CropBits(12) == VarLenNumber(2, 0x089au));
  assert(x.CropBits(22) == VarLenNumber(3, 0x16789au));
  assert(x.CropBits(25) == VarLenNumber(4, 0x0056789au));
  assert(x.CropBits(30) == VarLenNumber(4, 0x0056789au));
  assert(x.CropBits(63) == VarLenNumber(8, (uint64_t)0x56789aull));

  VarLenNumber y = VarLenNumber::AllPositive(1000);
  assert(y.CropBits(1200) == VarLenNumber::AllPositive(1000));
  assert(y.CropBits(500)  == VarLenNumber::AllPositive(500));
}

void TEST_Point() {
  VarLenPoint2D point1(VarLenNumber(1, 0x12u), VarLenNumber(1, 0x34u));
  VarLenPoint2D point2(VarLenNumber(1, 0x56u), VarLenNumber(3, 0x000078u));
  VarLenPoint2D point3(VarLenNumber(3, 0xffff9au), VarLenNumber(1, 0xbcu));

  assert(point1.GetX().GetLength() == 1);
  assert(point1.GetY().GetLength() == 1);
  assert(point1.GetX() == VarLenNumber(1, 0x12u));
  assert(point1.GetY() == VarLenNumber(1, 0x34u));

  assert(point2.GetX().GetLength() == 3);
  assert(point2.GetY().GetLength() == 3);
  assert(point2.GetX() == VarLenNumber(1, 0x56u));
  assert(point2.GetY() == VarLenNumber(1, 0x78u));

  assert(point3.GetX().GetLength() == 3);
  assert(point3.GetY().GetLength() == 3);
  assert(point3.GetX() == VarLenNumber(3, 0xffff9au));
  assert(point3.GetY() == VarLenNumber(1, 0xbcu));
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(Default);
  TEST(FromString);
  TEST(FromStringIncorrectLength);
  TEST(FromInteger);
  TEST(VeryLong);
  TEST(LengthOnlyConstructor);
  TEST(NumericalStringRepr);
  TEST(NumericalValue);
  TEST(GetByteAt);
  TEST(CropOrResize);
  TEST(Compare);
  TEST(Add);
  TEST(AndOrXor);
  TEST(AndOrXorAssign);
  TEST(AndOrXorDifferentLength);
  TEST(AllPositive);
  TEST(RShift);
  TEST(IsBitSetAt);
  TEST(ModifyBitAt);
  TEST(InvertNLowestBits);
  TEST(CropBits);
  TEST(Point);

  return 0;
}
