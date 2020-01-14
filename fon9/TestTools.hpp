﻿/// \file fon9/TestTools.hpp
///
/// fon9 的簡易測試工具.
///
/// \author fonwinz@gmail.com
#ifndef __fon9_TestTools_hpp__
#define __fon9_TestTools_hpp__
#include "fon9/Utility.hpp"

fon9_BEFORE_INCLUDE_STD;
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
fon9_AFTER_INCLUDE_STD;

namespace fon9 {

#ifdef _MSC_VER
   #define fon9_NOINLINE(s)   __declspec(noinline) s
#elif __GNUC__
   #define fon9_NOINLINE(s)   s __attribute__ ((noinline))
#else
   #define fon9_NOINLINE(s)   s
#endif

class AutoTimeUnit {
   double      Span_;
   const char* UnitStr_;
public:
   AutoTimeUnit(double secs) {
      this->Make(secs);
   }

   void Make(double secs) {
      static const char* cstrUnits[] = {
         "sec", "ms", "us", "ns"
      };
      bool isNeg = (secs < 0);
      if (isNeg)
         secs = -secs;
      size_t uidx = 0;
      while(0 < secs && secs < 1) {
         if (++uidx >= numofele(cstrUnits)) {
            --uidx;
            break;
         }
         secs *= 1000;
      }
      this->Span_ = isNeg ? (-secs) : secs;
      this->UnitStr_ = cstrUnits[uidx];
   }
   
   friend std::ostream& operator<<(std::ostream& os, const AutoTimeUnit& tu) {
      os << std::setw(static_cast<int>(os.precision()) + 4) << tu.Span_ << " " << tu.UnitStr_;
      return os;
   }
};

class StopWatch {
   using Clock = std::chrono::high_resolution_clock;
   Clock::time_point StartTime_;
public:
   StopWatch() {
      this->ResetTimer();
   }

   void ResetTimer() {
      this->StartTime_ = Clock::now();
   }

   template <class Period = std::ratio<1>>
   double CurrSpan() {
      using Span = std::chrono::duration<double, Period>;
      Span  span = std::chrono::duration_cast<Span>(Clock::now() - this->StartTime_);
      return span.count();
   }

   template <class Period = std::ratio<1>>
   double StopTimer() {
      auto span = this->CurrSpan();
      this->ResetTimer();
      return span;
   }

   static std::ostream& PrintResultNoEOL(double span, const char* msg, uint64_t timesRun) {
      return std::cout << msg << ": "
         << span << " secs / " << timesRun << " times = "
         << AutoTimeUnit{timesRun ? (span / static_cast<double>(timesRun)) : 0};
   }
   static std::ostream& PrintResult(double span, const char* msg, uint64_t timesRun) {
      PrintResultNoEOL(span, msg, timesRun);
      return std::cout << std::endl;
   }

   std::ostream& PrintResultNoEOL(const char* msg, uint64_t timesRun) {
      return this->PrintResultNoEOL(this->StopTimer(), msg, timesRun);
   }
   std::ostream& PrintResult(const char* msg, uint64_t timesRun) {
      return this->PrintResult(this->StopTimer(), msg, timesRun);
   }
};

struct AutoPrintTestInfo {
   const char* TestName_;

   AutoPrintTestInfo(const char* testName) : TestName_(testName) {
      std::cout <<
         "#####################################################\n"
         "fon9 [" << testName << "] test\n"
         "====================================================="
         << std::endl;
      std::cout << std::fixed << std::setprecision(9);
      std::cout.imbue(std::locale(""));
   }

   ~AutoPrintTestInfo() {
      std::cout <<
         "=====================================================\n"
         "fon9 [" << this->TestName_ << "] test # END #\n"
         "#####################################################\n"
         << std::endl;
   }

   void PrintSplitter() {
      std::cout << "-----------------------------------------------------" << std::endl;
   }
};

//--------------------------------------------------------------------------//

inline void CheckTestResult(const char* testItem, const char* fileLn, bool ok) {
   std::cout << (ok ? "[OK   ]" : "[ERROR]") << " TestItem=" << testItem;
   if (ok) {
      std::cout << std::endl;
      return;
   }
   std::cout << "|err=@" << fileLn << std::endl;
   abort();
}

#define fon9_CheckTestResult(testItem, ok) \
   fon9::CheckTestResult(testItem, __FILE__ ":" fon9_CTXTOCSTR(__LINE__), (ok))

//--------------------------------------------------------------------------//

static inline bool IsKeepTestFiles(int argc, char** argv) {
   for (int L = 1; L < argc; ++L) {
      if (strcmp(argv[L], "--keep") == 0)
         return true;
   }
   return false;
}
static inline bool WaitRemoveFile(const char* fileName) {
   unsigned count = 100;
   // 可能使用 AsyncFileAppender, 所以可能仍有部分訊息尚未完全寫入(尚未關檔).
   // 因此 remove() 可能失敗, 所以在此使用 while(remove()) 直到成功.
   while (remove(fileName) != 0) {
      if (--count <= 0) {
         std::cout << "FileName=" << fileName;
         perror("|Remove test file error:");
         return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
   }
   return true;
}

} // namespace
#endif//__fon9_TestTools_hpp__
