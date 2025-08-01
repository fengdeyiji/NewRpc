#include <format>
#include <memory>
#include "mechanism/serialization.hpp"
#include "mechanism/stringification.hpp"
#include "coroutine_framework/net_module/rpc_struct.h"
#include <boost/test/unit_test.hpp>

using namespace ToE;
using namespace std;

struct Person {
  bool operator==(const Person &rhs) const { return name == rhs.name && age == rhs.age; }
  std::string name;
  int32_t age; 
};
STATIC_REFLECT(Person, name, age);

struct Company {
  Company() = default;
  Company(std::string name,
          std::vector<Person> employees,
          double profit)
  : name_(std::move(name)),
  employees_(std::move(employees)),
  profit_(profit) {}
private:
  std::string name_;
  std::vector<Person> employees_;
  double profit_;
};

#define SERIALIZE(obj) \
pos = 0;\
Serializer<decltype(obj)>::serialize(obj, buffer, buffer_len, pos)

#define DESERIALIZE(obj) \
pos = 0;\
Serializer<decltype(obj)>::deserialize(obj, buffer, buffer_len, pos)

#define GET_SERIALIZE_SIZE(obj) \
Serializer<decltype(obj)>::get_serialize_size(obj)

struct Fixture {
  static constexpr int64_t buffer_len = 4096;
  Fixture() :
  buffer(new std::byte[buffer_len]),
  pos(0) {}
  ~Fixture() { delete[] buffer; }
  std::byte *buffer;
  int64_t pos;
};

BOOST_FIXTURE_TEST_SUITE(test_serialization, Fixture)

BOOST_AUTO_TEST_CASE(test_serialize_basic) {
  {
    int8_t var1 = 1;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 1);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    int8_t var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
  {
    uint16_t var1 = 1;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    uint16_t var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
  {
    int32_t var1 = 1;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 4);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    int32_t var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
  {
    uint64_t var1 = 1;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 8);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    uint64_t var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
  {
    double var1 = 2.34;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 8);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    double var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
}

BOOST_AUTO_TEST_CASE(test_serialize_pointer) {
  {
    int8_t *var1 = nullptr;
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 1);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    int8_t *var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(var1, var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
  {
    int16_t *var1 = new int16_t(1);
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 3);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    int16_t *var2;
    DESERIALIZE(var2);
    BOOST_CHECK_EQUAL(*var1, *var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    delete var1;
    delete var2;
  }
}

// template <typename T>
// constexpr int static_print( ) {
//   constexpr int64_t idx = 0;
//   for (; idx < GlobalReflectMap<Person>::MemberCount; ) {
//     std::byte *ptr = (std::byte *)&person + GlobalReflectMap<Person>::MemberOffset[idx];

//   }
// }

BOOST_AUTO_TEST_CASE(test_serialize_struct) {
  {
    Person var1{"coolteng", 30};
    SERIALIZE(var1);
    BOOST_CHECK_EQUAL(pos, 20);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
    Person var2;
    DESERIALIZE(var2);
    BOOST_CHECK(var1 == var2);
    BOOST_CHECK_EQUAL(pos, GET_SERIALIZE_SIZE(var1));
  }
}

BOOST_AUTO_TEST_SUITE_END()