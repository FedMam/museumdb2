#pragma once

#include "rocksdb/rocksdb_namespace.h"
#include "hilbert/geometry.h"

namespace ROCKSDB_NAMESPACE {

// This function computes the distance of the point on a Hilbert curve
// built in the [0, 2^N)x[0, 2^N) square, where N is the length
// in bits of the two coordinates of the point.
VarLenNumber PointToHilbertValue(const VarLenPoint2D& point);

// This function accepts the distance of a point on a Hilbert curve and
// returns the coordinates of that point. The Hilbert curve is assumed
// to have been built in the [0, 2^N)x[0, 2^N) square, where N is the length
// of hilbertValue in bits divided by two.
//
// Warning: throws an error if hilbertValue's length in *bytes* (GetLength()) is not even.
// This is because this function is only intended to be used with Hilbert values produced
// by PointToHilbertValue(), whose byte length is always even.
VarLenPoint2D HilbertValueToPoint(const VarLenNumber& hilbertValue);

// A wrapper of VarLenPoint2D that automatically computes the Hilbert value
// for that point.
struct VarLenPoint2DWithHilbertValue {
  VarLenPoint2DWithHilbertValue(const VarLenPoint2D& point)
    : point_(point),
      hilbert_value_(PointToHilbertValue(point)) { }
  
  inline const VarLenPoint2D& GetPoint() const { return point_; }

  inline const VarLenNumber& GetHilbertValue() const { return hilbert_value_; }

  inline size_t GetNumberLength() const { return point_.GetX().GetLength(); }

 private:
  VarLenPoint2D point_;
  VarLenNumber hilbert_value_;
};

}
