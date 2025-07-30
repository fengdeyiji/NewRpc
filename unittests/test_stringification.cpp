#include <format>
#include <memory>
#include <boost/test/unit_test.hpp>
#include "coroutine_framework/framework.hpp"
#include "mechanism/stringification.hpp"
#include <boost/regex.hpp>

using namespace G;
using namespace std;

struct Person { std::string name; int age; };
template<>
struct std::formatter<Person> {
  constexpr auto parse(auto& ctx) { return ctx.end(); }
  auto format(const Person& p, auto& ctx) const {
    return std::format_to(ctx.out(), "{} (Age: {})", p.name, p.age);
  }
};

namespace test {
struct Company {
  friend struct REFLECT<Company>;
  Company(std::string name,
          std::vector<Person> employees,
          double profit)
  : name_(std::move(name)),
  employees_(std::move(employees)),
  profit_(profit) {}
  std::string name_;
  std::vector<Person> employees_;
  double profit_;
};
}
STATIC_REFLECT(test::Company, name_, employees_, profit_);

struct GlobalSetup {
  GlobalSetup() { G::GlobalInit(LogLevel::info); }
  ~GlobalSetup() { spdlog::drop_all(); }
};
BOOST_GLOBAL_FIXTURE(GlobalSetup);
struct Fixture {
  static constexpr int64_t buffer_len = 4_KiB;
  Fixture() :
  buffer(new char[buffer_len]),
  pos(0) {}
  ~Fixture() {}
  char *buffer;
  int64_t pos;
};
BOOST_FIXTURE_TEST_SUITE(test_stringification, Fixture)

#define TO_STRING(obj) \
pos = 0;\
value_to_string(obj, buffer, buffer_len, pos);\

BOOST_AUTO_TEST_CASE(test_print_basic) {
  { // int8_t 与 char类型的区分：int8_t被识别为signed char类型，与char被分别当作两种类型对待
    int8_t var = 1;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "1");
  }
  { // 浮点数类型保留两位小数
    double var = 4.0;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "4.00");
  }
  { // 多于两位小数只保留两位
    double var = 4.123;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "4.12");
  }
  { // 字符类型，加一个单引号
    char var = 'x';
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "'x'");
  }
  { // 布尔类型打印true和false（format默认行为）
    bool var = true;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "true");
  }
}

BOOST_AUTO_TEST_CASE(test_print_string) {
  { // std::string
    std::string var = "abc";
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "abc");
  }
  { // 静态字符串
    const char *var = "abc";
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "abc");
  }
  { // 字符串数组
    char var[4] = "abc";
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "abc");
  }
  { // string_view
    string_view var("abc");
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "abc");
  }
}

BOOST_AUTO_TEST_CASE(test_print_pointer) {
  { // 整形指针
    int *var = new int(1);
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("\\(0x[0-9a-f]+\\):1");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
    delete var;
  }
  { // 整形的非一级指针
    int **var = new int*(new int(1));
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("\\(0x[0-9a-f]+\\):\\(0x[0-9a-f]+\\):1");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
    delete *var;
    delete var;
  }
  { // char *的非一级指针
    const char **var = new const char *("abc");
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("\\(0x[0-9a-f]+\\):abc");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
    delete var;
  }
  { // check nullptr
    const char **var = nullptr;
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("NULL");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
  }
  { // check nullptr
    int *var = nullptr;
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("NULL");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
  }
  { // check nullptr
    void *var = nullptr;
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("NULL");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
  }
  { // 泛指针类型
    std::shared_ptr<int> var = std::make_shared<int>(1);
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("\\(0x[0-9a-f]+\\):1");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
  }
  { // 有效的void*类型不支持解引用，仅打印目标地址
    int var_ = 1;
    void *var = &var;
    TO_STRING(var);
    buffer[pos] = '\0';
    boost::regex ptr_addr_regex("0x[0-9a-f]+");
    BOOST_CHECK_MESSAGE(boost::regex_match(buffer, ptr_addr_regex), buffer);
  }
}

enum class TEST_ENUM {
  ENUM1 = 1,
};
BOOST_AUTO_TEST_CASE(test_print_enum) {
  { // 枚举
    TEST_ENUM var = TEST_ENUM::ENUM1;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "1");
  }
}

struct TestClass {
  void to_string(char *buffer, const int64_t buffer_len, int64_t &pos) const {
    pos += std::format_to_n(buffer + pos, buffer_len - pos, "hello print").size;
  }
};
static_assert(ToStringAble<TestClass>, "TestClass not satisfied");
BOOST_AUTO_TEST_CASE(test_print_to_string_able) {
  { // 自定义类
    TestClass var;
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "hello print");
  }
}

BOOST_AUTO_TEST_CASE(test_print_formattable) {
  static_assert(std::formattable<Person, char>, "Person not satisfied.");
  { // 可格式化类型
    Person var{"CoolTeng", 30};
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "CoolTeng (Age: 30)");
  }
}

BOOST_AUTO_TEST_CASE(test_print_ranges) {
  { // vector with few
    vector<int> var{1, 2, 3};
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "[1, 2, 3]");
  }
  { // vector with more
    vector<int> var{1, 2, 3, 4, 5, 6, 7, 8, 9};
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "[1, 2, 3, 4, 5, 6, 7, 8..(1 more)]");
  }
  { // raw array
    double var[9]{1, 2, 3, 4, 5, 6, 7, 8, 9};
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "[1.00, 2.00, 3.00, 4.00, 5.00, 6.00, 7.00, 8.00..(1 more)]");
  }
}

BOOST_AUTO_TEST_CASE(test_reflectable) {
  { // hana reflectable
    static_assert(Reflectable<test::Company>);
    int a;
    test::Company var{"Tecent", {
    {"CoolTeng0", 30},
    {"CoolTeng1", 31},
    {"CoolTeng2", 32},
    {"CoolTeng3", 33},
    {"CoolTeng4", 34},
    {"CoolTeng5", 35},
    {"CoolTeng6", 36},
    {"CoolTeng7", 37},
    {"CoolTeng8", 38},
    {"CoolTeng9", 39}}, 123456789.123};
    TO_STRING(var);
    buffer[pos] = '\0';
    BOOST_CHECK_EQUAL(buffer, "name_:Tecent, employees_:{[{CoolTeng0 (Age: 30)}, {CoolTeng1 (Age: 31)}, {CoolTeng2 (Age: 32)}, {CoolTeng3 (Age: 33)}, {CoolTeng4 (Age: 34)}, {CoolTeng5 (Age: 35)}, {CoolTeng6 (Age: 36)}, {CoolTeng7 (Age: 37)}..(2 more)]}, profit_:123456789.12");
  }
}

BOOST_AUTO_TEST_CASE(test_buffer_not_enough) {
  static constexpr int64_t buffer_len = 8;
  char buffer[buffer_len];
  { // vector with few
    vector<int> var{1, 2, 3};
    TO_STRING(var);
    BOOST_CHECK_EQUAL(pos, buffer_len);
    buffer[pos - 1] = '\0';
    BOOST_CHECK_EQUAL(buffer, "[1, 2, ");
  }
}

BOOST_AUTO_TEST_SUITE_END()