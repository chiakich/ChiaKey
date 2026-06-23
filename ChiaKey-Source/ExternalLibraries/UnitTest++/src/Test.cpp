#include "Test.h"

#include "AssertException.h"
#include "Config.h"
#include "ExecuteTest.h"
#include "MemoryOutStream.h"
#include "TestList.h"
#include "TestResults.h"

#ifdef UNITTEST_POSIX
#include "Posix/SignalTranslator.h"
#endif

namespace UnitTest {

TestList& Test::GetTestList() {
  static TestList s_list;
  return s_list;
}

Test::Test(char const* testName, char const* suiteName, char const* filename,
           int const lineNumber)
    : m_details(testName, suiteName, filename, lineNumber),
      next(0),
      m_timeConstraintExempt(false) {}

Test::~Test() {}

void Test::Run(TestResults& testResults) {
  ExecuteTest(*this, testResults, m_details);
}

void Test::RunImpl(TestResults&) const {}

}  // namespace UnitTest
