// Unit test framework.
//
// Copyright (c) 2021-2026 Johannes Overmann
//
// Distributed under the Boost Software License, Version 1.0.

#ifdef ENABLE_UNIT_TEST

#include "UnitTest.hpp"
#include <cstdlib>

UnitTestRegistry* unitTestRegistry{};

static UnitTestRegistry* getUnitTestRegistry()
{
    if (unitTestRegistry == nullptr)
    {
        unitTestRegistry = new UnitTestRegistry();
    }
    return unitTestRegistry;
}

UnitTest::UnitTest(const std::string& testName_, const std::string& testFile_, int testLine_)
: testName(testName_)
, testFile(testFile_)
, testLine(testLine_)
{
    UnitTestRegistry::registerTest(this);
}

UnitTest::~UnitTest() = default;

void UnitTestRegistry::registerTest(UnitTest* test)
{
    getUnitTestRegistry()->tests[test->testName] = test;
}

int UnitTestRegistry::runTests()
{
    size_t maxNameLen = 0;
    for (const auto& kv: getUnitTestRegistry()->tests)
    {
        maxNameLen = std::max(maxNameLen, kv.second->getTestName().length());
    }
    size_t numTests = 0;
    for (const auto& kv: getUnitTestRegistry()->tests)
    {
        std::cout << "Test " << kv.second->getTestName() << std::string(maxNameLen - kv.second->getTestName().length() + 1, ' ');
        kv.second->run();
        std::cout << "OK\n";
        numTests++;
    }
    std::cout << "--\nAll " << numTests << " tests passed\n";
    return 0;
}

void UNIT_TEST_RUN()
{
    UnitTestRegistry::runTests();
    std::exit(0);
}

#endif
