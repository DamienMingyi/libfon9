﻿/// \file fon9/Named.cpp
/// \author fonwinz@gmail.com
#include "fon9/Named.hpp"
#include "fon9/StrTools.hpp"
#include "fon9/RevPrint.hpp"

namespace fon9 {

fon9_API const char* FindInvalidNameChar(StrView str) {
   const char* pend = str.end();
   const char* pbeg = str.begin();
   if (pbeg == pend || !IsValidName1stChar(*pbeg))
      return pbeg;
   while (++pbeg != pend) {
      if (!IsValidNameChar(*pbeg))
         return pbeg;
   }
   return nullptr;
}

fon9_API Named DeserializeNamed(StrView& cfg, char chSpl, int chTail, StrView* exNameParam) {
   StrView  descr = chTail == -1 ? cfg : SbrFetchNoTrim(cfg, static_cast<char>(chTail));
   StrView  name  = SbrFetchNoTrim(descr, chSpl);
   StrView  title = SbrFetchNoTrim(descr, chSpl);
   
   StrTrim(&name);
   if (const char* pInvalid = FindInvalidNameChar(name)) {
      if (exNameParam == nullptr || pInvalid == name.begin()) {
         cfg.SetBegin(pInvalid);
         return Named{};
      }
      exNameParam->Reset(pInvalid, name.end());
      name.SetEnd(pInvalid);
   }
   if (!cfg.empty())
      cfg.SetBegin(descr.end() + (chTail != -1));
   return Named(name.ToString(),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(title)),
      StrView_ToNormalizeStr(StrTrimRemoveQuotes(descr)));
}

fon9_API void SerializeNamed(std::string& dst, const Named& named, char chSpl, int chTail) {
   dst.append(named.Name_);
   if (!named.GetTitle().empty() || !named.GetDescription().empty()) {
      dst.push_back(chSpl);
      if (!named.GetTitle().empty()) {
         dst.push_back('\'');
         StrView_ToEscapeStr(dst, &named.GetTitle());
         dst.push_back('\'');
      }
      if (!named.GetDescription().empty()) {
         dst.push_back(chSpl);
         dst.push_back('\'');
         StrView_ToEscapeStr(dst, &named.GetDescription());
         dst.push_back('\'');
      }
   }
   if (chTail != -1)
      dst.push_back(static_cast<char>(chTail));
}

//--------------------------------------------------------------------------//

static bool RevPrintEscapeStr(RevBuffer& rbuf, fon9::StrView str, char chSpl) {
   if (str.empty())
      return false;
   std::string dst = StrView_ToEscapeStr(str);
   if (dst.size() == str.size() && (chSpl == '\0' || !str.Find(chSpl)))
      RevPrint(rbuf, str);
   else
      RevPrint(rbuf, '\'', dst, '\'');
   return true;
}

fon9_API void RevPrintNamedDesc(RevBuffer& rbuf, const Named& named, char chSpl) {
   const bool hasDesc = RevPrintEscapeStr(rbuf, &named.GetDescription(), '\0');
   if (hasDesc)
      RevPrint(rbuf, chSpl);
   if (RevPrintEscapeStr(rbuf, &named.GetTitle(), chSpl) || hasDesc)
      RevPrint(rbuf, chSpl);
}
fon9_API void RevPrintNamed(RevBuffer& rbuf, const Named& named, char chSpl) {
   RevPrintNamedDesc(rbuf, named, chSpl);
   RevPrint(rbuf, named.Name_);
}

} // namespaces
