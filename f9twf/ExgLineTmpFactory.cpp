﻿// \file f9twf/ExgLineTmpFactory.cpp
// \author fonwinz@gmail.com
#include "f9twf/ExgLineTmpFactory.hpp"

namespace f9twf {

ExgLineTmpFactory::ExgLineTmpFactory(std::string logPathFmt, Named&& name)
   : baseFactory(std::move(name))
   , basePathMaker{std::move(logPathFmt)} {
}
fon9::io::SessionServerSP ExgLineTmpFactory::CreateSessionServer(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   (void)mgr; (void)cfg;
   errReason = "f9twf::ExgLineTmpFactory|err=Not support Server.";
   return fon9::io::SessionServerSP{};
}
fon9::io::SessionSP ExgLineTmpFactory::CreateSession(fon9::IoManager& mgr, const fon9::IoConfigItem& cfg, std::string& errReason) {
   ExgIoManager* ioMgr = dynamic_cast<ExgIoManager*>(&mgr);
   if (ioMgr == nullptr) {
      errReason = "IoManager must be f9twf::ExgIoManager";
      return fon9::io::SessionSP{};
   }
   ExgLineTmpArgs lineArgs;
   errReason = ExgLineTmpArgsParser(lineArgs, ToStrView(cfg.SessionArgs_));
   if (!errReason.empty())
      return fon9::io::SessionSP{};

   std::string       logFileName;
   fon9::TimeStamp   tday;
   errReason = this->MakeLogPath(logFileName, &tday);
   if (!errReason.empty())
      return fon9::io::SessionSP{};
   fon9::NumOutBuf nbuf;
   logFileName.append("TWF_");
   logFileName.append(fon9::ToStrRev(nbuf.end(), TmpGetValue<TmpFcmId_t>(lineArgs.FcmId_)), nbuf.end());
   logFileName.push_back('_');
   fon9::FmtDef fmt{3, fon9::FmtFlag::IntPad0};
   logFileName.append(fon9::ToStrRev(nbuf.end(), TmpGetValue<TmpSessionId_t>(lineArgs.SessionId_), fmt), nbuf.end());
   logFileName.append(".bin");
   ExgLineTmpLog log;
   errReason = log.Open(lineArgs, std::move(logFileName), tday);
   if (errReason.empty())
      return this->CreateLineTmp(*ioMgr, lineArgs, std::move(log), errReason);
   return fon9::io::SessionSP{};
}

} // namespaces
