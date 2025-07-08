#include "serialization/serialization.hpp"
#include "stringification/stringification.hpp"
#include <iostream>

struct Point {
  Point() : x_(0), y_(0) {}
  Point(int x, int y) : x_(x), y_(y) {}
  int x_;
  int y_;
};
BOOST_HANA_ADAPT_STRUCT(Point, x_, y_);

struct Rectangle {
  Rectangle() : left_up_(), right_down_() {}
  Rectangle(const Point &left_up, const Point &right_down) : left_up_(left_up), right_down_(right_down) {}
  Point left_up_;
  Point right_down_;
};
BOOST_HANA_ADAPT_STRUCT(Rectangle, left_up_, right_down_);

struct MultiPtr {
  MultiPtr() {
    void_ptr_ = (void *)0x123;
    int_ptr_ = new int(2);
    shared_ptr_ = std::make_shared<std::string>("234");
    uniq_ptr_ = std::make_unique<double>(3.45);
  }
  void *void_ptr_;
  int *int_ptr_;
  std::shared_ptr<std::string> shared_ptr_;
  std::unique_ptr<double> uniq_ptr_;
};

int main() {
  
  std::vector<Rectangle> a;
  for (int idx = 0; idx < 16; ++idx) {
    a.push_back(Rectangle({idx, idx}, {idx + 1, idx + 1}));
  }
  constexpr int64_t buffer_len = 4096;
  char buffer[buffer_len];
  int64_t pos = 0;
  new_rpc::value_to_string(a, buffer, buffer_len, pos);
  buffer[pos] = '\0';
  // std::print("{:s}\n", (char *)buffer);
  // MultiPtr multi_ptr;
  // std::print("{:s}\n", (char *)buffer);
  return 0;
}