﻿// \file f9twf/TwfSymbRef.hpp
// \author fonwinz@gmail.com
#ifndef __f9twf_TwfSymbRef_hpp__
#define __f9twf_TwfSymbRef_hpp__
#include "f9twf/Config.h"
#include "fon9/fmkt/Symb.hpp"
#include "fon9/fmkt/FmktTypes.hpp"

namespace f9twf {

struct TwfSymbRef_Data {
   struct PriLmt {
      fon9::fmkt::Pri   Up_{};
      fon9::fmkt::Pri   Dn_{};
   };
   enum {
      kPriLmtCount = 3
   };

   fon9::fmkt::Pri   PriRef_{};
   PriLmt            PriLmts_[kPriLmtCount];
   char              PaddingAfterBaseRef___[6];
   /// LvUpLmt_ == 0 使用 PriLmts_[0];
   /// - 預告: -1 or -2;
   /// - 實施:  1 or  2;
   int8_t            LvUpLmt_{0};
   int8_t            LvDnLmt_{0};

   TwfSymbRef_Data() {
      this->Clear();
   }
   void Clear() {
      memset(this, 0, sizeof(*this));
   }
   bool IsEuqalBaseRef(const TwfSymbRef_Data& rhs) const {
      return memcmp(this, &rhs, fon9_OffsetOf(TwfSymbRef_Data, PaddingAfterBaseRef___)) == 0;
   }
};

// gcc 不喜歡把這行寫在 namespace f9twf 裡面, 所以就移到 namespace 外面, 避免 gcc 的警告.
// template class fon9::fmkt::SimpleSymbData<f9twf::TwfSymbRef_Data> f9twf_API;
using TwfSymbRef = fon9::fmkt::SimpleSymbData<TwfSymbRef_Data>;

f9twf_API fon9::seed::Fields TwfSymbRef_MakeFields();

} // namespaces
template class fon9::fmkt::SimpleSymbData<f9twf::TwfSymbRef_Data> f9twf_API;

#endif//__f9twf_TwfSymbRef_hpp__
