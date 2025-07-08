#include <format>
#include <memory>
#include <boost/test/unit_test.hpp>
#include "boost/test/tools/old/interface.hpp"
#include "serialization/serialization.hpp"
#include <boost/regex.hpp>

using namespace new_rpc;
using namespace std;

struct Person { std::string name; int age; };
template<>
struct std::formatter<Person> {
  constexpr auto parse(auto& ctx) { return ctx.end(); }
  auto format(const Person& p, auto& ctx) const {
    return std::format_to(ctx.out(), "{} (Age: {})", p.name, p.age);
  }
};

struct Company {
  friend struct boost::hana::accessors_impl<Company>;
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
BOOST_HANA_ADAPT_STRUCT(Company, name_, employees_, profit_);

#define SERIALIZE(obj) \
pos = 0;\
serialize(obj, buffer, buffer_len, pos)

#define DESERIALIZE(obj) \
pos = 0;\
deserialize(obj, buffer, buffer_len, pos)

#define GET_SERIALIZE_SIZE(obj) \
get_serialize_size(obj)

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

BOOST_AUTO_TEST_SUITE_END()