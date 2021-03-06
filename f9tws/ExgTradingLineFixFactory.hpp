﻿// \file f9tws/ExgTradingLineFixFactory.hpp
// \author fonwinz@gmail.com
#ifndef __f9tws_ExgTradingLineFixFactory_hpp__
#define __f9tws_ExgTradingLineFixFactory_hpp__
#include "f9tws/ExgTradingLineFix.hpp"
#include "f9tws/ExgLineFactory.hpp"

namespace f9tws {

class f9tws_API ExgTradingLineFixFactory : public ExgLineFactory {
   fon9_NON_COPY_NON_MOVE(ExgTradingLineFixFactory);
   using base = ExgLineFactory;
protected:
   f9fix::FixConfig  FixConfig_;

   fon9::io::SessionSP CreateTradingLine(ExgTradingLineMgr& lineMgr,
                                         const fon9::IoConfigItem& cfg,
                                         std::string& errReason) override;
   virtual fon9::io::SessionSP CreateTradingLineFix(ExgTradingLineMgr&           lineMgr,
                                                    const ExgTradingLineFixArgs& args,
                                                    f9fix::IoFixSenderSP         fixSender) = 0;
public:
   /// 衍生者必須自行處理相關訊息:
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_ExecutionReport).FixMsgHandler_ = &OnFixExecutionReport;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelReject).FixMsgHandler_ = &OnFixCancelReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_NewOrderSingle).FixRejectHandler_ = &OnFixReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderReplaceRequest).FixRejectHandler_ = &OnFixReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderCancelRequest).FixRejectHandler_ = &OnFixReject;
   /// this->FixConfig_.Fetch(f9fix_kMSGTYPE_OrderStatusRequest).FixRejectHandler_ = &OnFixReject;
   ExgTradingLineFixFactory(std::string fixLogPathFmt, Named&& name);
};

} // namespaces
#endif//__f9tws_ExgTradingLineFixFactory_hpp__
