// Unit test framework.
//
// Copyright (c) 2021-2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#pragma once

#include <cassert>
#include <iostream>
#include <map>
#include <string>

#ifdef ENABLE_UNIT_TEST

#undef NDEBUG

struct UnitTest;

class UnitTestRegistry
{
public:
    static void registerTest(UnitTest* test);
    static int runTests();

private:
    std::map<std::string, UnitTest*> tests;
};

struct UnitTest
{
    UnitTest(const std::string& testName_, const std::string& testFile_, int testLine_);
    virtual ~UnitTest();
    virtual void run() = 0;
    std::string getTestName() const { return testFile + ":" + testName; }
    std::string testName;
    std::string testFile;
    int testLine{};
};

void UNIT_TEST_RUN();

# define UNIT_TEST(name)                  \
  struct UnitTest_##name: public UnitTest \
  {                                       \
   UnitTest_##name()                      \
   : UnitTest(#name, __FILE__, __LINE__)  \
   {                                      \
   }                                      \
   virtual void run() override;           \
  } UnitTest_instance_##name;             \
  inline void UnitTest_##name::run()

#else

# define UNIT_TEST(name) \
  class UnitTest_##name  \
  {                      \
   void run();           \
  };                     \
  inline void UnitTest_##name::run()
# define UNIT_TEST_RUN() \
  do                     \
  {                      \
  } while (false)

#endif

#define ASSERT_EQ(a, b)                                                                 \
 {                                                                                      \
  if ((a) != (b))                                                                       \
  {                                                                                     \
   std::cout << "\nError: ASSERT_EQ(" << ut1::toStr(a) << ", " << ut1::toStr(b) << ") failed!\n"; \
   assert((a) == (b));                                                                  \
  }                                                                                     \
 }

#define ASSERT_NE(a, b)                                                                 \
 {                                                                                      \
  if ((a) == (b))                                                                       \
  {                                                                                     \
   std::cout << "\nError: ASSERT_NE(" << ut1::toStr(a) << ", " << ut1::toStr(b) << ") failed!\n"; \
   assert((a) != (b));                                                                  \
  }                                                                                     \
 }
