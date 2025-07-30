#pragma once
#include "mechanism/static_reflection.hpp"

namespace G
{

struct Point {
  Point() : x_{0}, y_{0} {}
  Point(int64_t x, int64_t y) : x_{x}, y_{y} {}
  Point(const Point &) = default;
  Point& operator=(const Point &) = default;
  Point operator+(const Point &rhs) {
    return {x_ + rhs.x_, y_ + rhs.y_};
  }
  int64_t x_;
  int64_t y_;
};

}
STATIC_REFLECT(G::Point, x_, y_)