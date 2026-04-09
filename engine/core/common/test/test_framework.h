#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Fabrica::Core::Test {

/**
 * Collects and executes registered test cases for lightweight unit tests.
 *
 * Registration follows an Observer-like pattern through static `Registrar`
 * instances created by macros.
 */
class Registry {
 public:
  /// Return the process-wide registry singleton.
  static Registry& Instance() {
    static Registry instance;
    return instance;
  }

  /**
   * Register a named test callback.
   *
   * @param name Test case label shown in diagnostics.
   * @param fn Test function to execute.
   */
  void Add(std::string name, std::function<void()> fn) {
    tests_.push_back({std::move(name), std::move(fn)});
  }

  /**
   * Execute all registered tests and return failure count.
   *
   * @return Total number of failed tests/checks.
   */
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

  /**
   * Record a failed expectation emitted by assertion macros.
   *
   * @param expression Original check expression text.
   * @param file Source file containing the failed check.
   * @param line Source line containing the failed check.
   * @param message Optional diagnostic details.
   */
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

/**
 * Registers one test function in the global `Registry` at static init time.
 */
class Registrar {
 public:
  Registrar(std::string name, std::function<void()> fn) {
    Registry::Instance().Add(std::move(name), std::move(fn));
  }
};

}  // namespace Fabrica::Core::Test

/**
 * Declare and register a test function.
 */
#define FABRICA_TEST(name)                                                   \
  static void name();                                                        \
  static ::Fabrica::Core::Test::Registrar name##_registrar_instance(#name,  \
                                                                     &name); \
  static void name()

/**
 * Assert that an expression evaluates to true.
 */
#define FABRICA_EXPECT_TRUE(expression)                                        \
  do {                                                                         \
    if (!(expression)) {                                                       \
      ::Fabrica::Core::Test::Registry::Instance().FailCheck(#expression,       \
                                                            __FILE__, __LINE__, \
                                                            "");               \
    }                                                                          \
  } while (false)

/**
 * Assert equality between two expressions.
 */
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
