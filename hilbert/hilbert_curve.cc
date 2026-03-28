#include "hilbert/hilbert_curve.h"

#include <assert.h>

// DEBUG
// #include <iostream>

namespace ROCKSDB_NAMESPACE {

namespace {

enum Rotation {
  k0 = 0,
  k90 = 1,
  k180 = 2,
  k270 = 3,
  kInvalid = 4
};

inline Rotation RotateRight(Rotation rot) {
  switch (rot) {
    case k0:   return k90;
    case k90:  return k180;
    case k180: return k270;
    case k270: return k0;
    default: return kInvalid;
  }
}

inline Rotation RotateLeft(Rotation rot) {
  switch (rot) {
    case k0:   return k270;
    case k90:  return k0;
    case k180: return k90;
    case k270: return k180;
    default: return kInvalid;
  }
}

inline Rotation Rotate180(Rotation rot) {
  switch (rot) {
    case k0:   return k180;
    case k90:  return k270;
    case k180: return k0;
    case k270: return k90;
    default: return kInvalid;
  }
}

inline Rotation FlipRotation(Rotation rot) {
  switch (rot) {
    case k0:   return k0;
    case k90:  return k270;
    case k180: return k180;
    case k270: return k90;
    default: return kInvalid;
  }
}

inline int RotateFlipSubQuadrant(int sub_quadrant, bool flip, Rotation rot) {
  // Flips and rotates the following U-shape:
  // 03
  // 12
  // and returns the sub-quadrant of the rotated shape that is in place of sub_quadrant of the original shape
  // (see below for details)

  switch (rot) {
    case k0:   break;
    case k90:  sub_quadrant = (sub_quadrant + 1) & 0b11; break;
    case k180: sub_quadrant = (sub_quadrant + 2) & 0b11; break;
    case k270: sub_quadrant = (sub_quadrant + 3) & 0b11; break;
    default: return -1;
  }
  if (flip) sub_quadrant = 3 - sub_quadrant;
  return sub_quadrant;
}

inline int ReverseRotateFlipSubQuadrant(int sub_quadrant, bool flip, Rotation rot) {
  // Flips and rotates the following U-shape:
  // 03
  // 12
  // and returns the sub-quadrant of the rotated shape that is in place of sub_quadrant of the original shape
  // (see below for details)

  if (flip) sub_quadrant = 3 - sub_quadrant;
  switch (rot) {
    case k0:   break;
    case k90:  sub_quadrant = (sub_quadrant + 3) & 0b11; break;
    case k180: sub_quadrant = (sub_quadrant + 2) & 0b11; break;
    case k270: sub_quadrant = (sub_quadrant + 1) & 0b11; break;
    default: return -1;
  }
  return sub_quadrant;
}

}

VarLenNumber PointToHilbertValue(const VarLenPoint2D& point) {
  // We recursively divide the unit square into quadrants.
  // At each iteration of the Hilbert curve, each quadrant is divided into four quadrant.
  // In each quadrant, the current iteration of the Hilbert curve can be
  // represented as U-shape rotated one of four angles and possibly flipped.
  // ('Flipped' means that the curve is going from the 'right' branch of the
  // U-shape to the 'left' one instead of from left to right)/
  // The rotation of the U-shape in the quadrant determines the
  // rotation of the U-shape in its sub-quadrants.
  // The position of the quadrant containing the point in its super-quadrant based on its rotation
  // determines two bits of the Hilbert code.
  // We'll number the subquadrants as shown below.
  // +-+-+ ┏┓┏┓      ━┓┏━
  // |0|3| ┃┃┃┃ ---> ┏┛┗┓
  // +-+-+ ┃┗┛┃      ┃┏┓┃
  // |1|2| ┗━━┛      ┗┛┗┛
  // +-+-+
  //        ^ this is U rotated 0 degrees (clockwise).
  // The rotations in sub-quadrants are: 270, 0, 0 and 90 degrees (subq. 0 to subq. 3).
  // Also the U shape in sub-quadrants 0 and 3 is flipped.

  size_t length = point.GetX().GetLength();  // by definition of VarLenPoint2D, it is equal to point.GetY().GetLength();
  VarLenNumber code(length * 2);

  int current_bit = length * 8 - 1;
  Rotation current_rotation = k0;
  bool current_flip = false;

  while (current_bit >= 0) {
    Rotation next_rotation;
    bool next_flip;
    int point_bits = (point.GetX().IsBitSetAt(current_bit) ? 0b10 : 0b00) + (point.GetY().IsBitSetAt(current_bit) ? 0b01 : 0b00);
    
    int sub_quadrant = 0;
    if (point_bits == 0b10)
      sub_quadrant = 3;
    else if (point_bits == 0b01)
      sub_quadrant = 1;
    else if (point_bits == 0b11)
      sub_quadrant = 2;

    // DEBUG
    // int sub_quadrant_for_point_bits = sub_quadrant;

    sub_quadrant = RotateFlipSubQuadrant(sub_quadrant, current_flip, current_rotation);

    // DEBUG
    // std::cout<<point.GetX().NumericalValue32()<<","<<point.GetY().NumericalValue32()<<current_bit<<" "<<current_flip<<" "<<current_rotation<<" "<<sub_quadrant<<" "<<sub_quadrant_for_point_bits<<std::endl;

    switch(sub_quadrant) {
      case 0:
        next_rotation = (!current_flip) ? RotateLeft(current_rotation) : RotateRight(current_rotation);
        next_flip = !current_flip;
        break;
      case 1:
      case 2:
        next_rotation = current_rotation;
        next_flip = current_flip;
        break;
      case 3:
        next_rotation = (!current_flip) ? RotateRight(current_rotation) : RotateLeft(current_rotation);
        next_flip = !current_flip;
        break;
    }

    switch (sub_quadrant) {
      case 0:
        break;
      case 1:
        code.SetBitAt(2 * current_bit);
        break;
      case 2:
        code.SetBitAt(2 * current_bit + 1);
        break;
      case 3:
        code.SetBitAt(2 * current_bit);
        code.SetBitAt(2 * current_bit + 1);
        break;
    }

    current_rotation = next_rotation;
    current_flip = next_flip;
    --current_bit;
  }

  return code;
}

VarLenPoint2D HilbertValueToPoint(const VarLenNumber& hilbertValue) {
  // Does precisely the inversion of what the previous function does.

  assert(hilbertValue.GetLength() % 2 == 0);
  size_t length = hilbertValue.GetLength() / 2;
  VarLenNumber x(length), y(length);

  int current_bit = length * 8 - 1;
  Rotation current_rotation = k0;
  bool current_flip = false;

  while (current_bit >= 0) {
    int sub_quadrant = (hilbertValue.IsBitSetAt(2 * current_bit + 1) ? 0b10 : 0b00) + (hilbertValue.IsBitSetAt(2 * current_bit) ? 0b01 : 0b00);
    
    Rotation next_rotation = current_rotation;
    bool next_flip = current_flip;

    switch(sub_quadrant) {
      case 0:
        next_rotation = (!current_flip) ? RotateLeft(current_rotation) : RotateRight(current_rotation);
        next_flip = !current_flip;
        break;
      case 1:
      case 2:
        next_rotation = current_rotation;
        next_flip = current_flip;
        break;
      case 3:
        next_rotation = (!current_flip) ? RotateRight(current_rotation) : RotateLeft(current_rotation);
        next_flip = !current_flip;
        break;
    }

    int sub_quadrant_for_point_bits = ReverseRotateFlipSubQuadrant(sub_quadrant, current_flip, current_rotation);

    // DEBUG
    // std::cout<<hilbertValue.NumericalValue32()<<current_bit<<" "<<current_flip<<" "<<current_rotation<<" "<<sub_quadrant<<" "<<sub_quadrant_for_point_bits<<std::endl;

    switch (sub_quadrant_for_point_bits) {
      case 0:
        break;
      case 1:
        y.SetBitAt(current_bit);
        break;
      case 2:
        x.SetBitAt(current_bit);
        y.SetBitAt(current_bit);
        break;
      case 3:
        x.SetBitAt(current_bit);
        break;
    }

    current_rotation = next_rotation;
    current_flip = next_flip;
    --current_bit;
  }

  return VarLenPoint2D(x, y);
}

}
