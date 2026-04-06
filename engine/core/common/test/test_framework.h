#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Fabrica::Core::Test {

class Registry {
 public:
  static Registry& Instance() {
    static Registry instance;
    return instance;
  }

  void Add(std::string name, std::function<void()> fn) {
    tests_.push_back({std::move(name), std::move(fn)});
  }

  int RunAll() {
    int failures = 0;
    for (const auto& test : tests_) {
      current_test_ = test.name;
      try {
        test.fn();
      } catch (const std::exception& exception) {
        ++failures;
        std::cerr << "[FAILED] " << test.name << " exception: " << exception.what()
                  << "\n";
      } catch (...) {
        ++failures;
        std::cerr << "[FAILED] " << test.name << " unknown exception\n";
      }
    }

    if (failed_checks_ > 0) {
      failures += failed_checks_;
    }

    if (failures == 0) {
      std::cout << "[PASSED] " << tests_.size() << " tests\n";
    }
    return failures;
  }

  void FailCheck(const char* expression, const char* file, int line,
                 const std::string& message) {
    ++failed_checks_;
    std::cerr << "[FAILED] " << current_test_ << " at " << file << ":" << line
              << " check: " << expression;
    if (!message.empty()) {
      std::cerr << " | " << message;
    }
    std::cerr << "\n";
  }

 private:
  struct TestCase {
    std::string name;
    std::function<void()> fn;
  };

  std::vector<TestCase> tests_;
  std::string current_test_;
  int failed_checks_ = 0;
};

class Registrar {
 public:
  Registrar(std::string name, std::function<void()> fn) {
    Registry::Instance().Add(std::move(name), std::move(fn));
  }
};

}  // namespace Fabrica::Core::Test

#define FABRICA_TEST(name)                                                   \
  static void name();                                                        \
  static ::Fabrica::Core::Test::Registrar name##_registrar_instance(#name,  \
                                                                     &name); \
  static void name()

#define FABRICA_EXPECT_TRUE(expression)                                        \
  do {                                                                         \
    if (!(expression)) {                                                       \
      ::Fabrica::Core::Test::Registry::Instance().FailCheck(#expression,       \
                                                            __FILE__, __LINE__, \
                                                            "");               \
    }                                                                          \
  } while (false)

#define FABRICA_EXPECT_EQ(left, right)                                       \
  do {                                                                        \
    if (!((left) == (right))) {                                               \
      std::ostringstream stream;                                              \
      stream << "left != right";                                              \
      ::Fabrica::Core::Test::Registry::Instance().FailCheck(#left " == " #right, \
                                                            __FILE__, __LINE__, \
                                                            stream.str());     \
    }                                                                         \
  } while (false)

