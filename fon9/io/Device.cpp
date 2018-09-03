﻿/// \file fon9/io/Device.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/Device.hpp"
#include "fon9/TimeStamp.hpp"
#include "fon9/DefaultThreadPool.hpp"

namespace fon9 { namespace io {

Device::~Device() {
   this->State_ = State::Destructing;
   this->CommonTimer_.StopAndWait();
   this->Session_->OnDevice_Destructing(*this);
   if (this->Manager_)
      this->Manager_->OnDevice_Destructing(*this);
}

void Device::Initialize() {
   assert(this->State_ == State::Initializing);
   this->Session_->OnDevice_Initialized(*this);
   if (this->Manager_)
      this->Manager_->OnDevice_Initialized(*this);
   this->State_ = State::Initialized;
}

void Device::MakeCallForWork() {
   DeviceSP pthis{this};
   GetDefaultThreadPool().EmplaceMessage([pthis]() {
      pthis->OpQueue_.TakeCall();
   });
}

void Device::OpThr_Open(Device& dev, std::string cfgstr) {
   State st = dev.State_;
   if (st >= State::Disposing)
      return;
   if (!cfgstr.empty()) // force reopen, use new config(cfgstr)
      return dev.OpImpl_Open(std::move(cfgstr));
   // 沒有 cfgstr, 檢查現在狀態是否允許 reopen.
   if (st == State::Listening ||
       st == State::LinkReady ||
       st == State::WaitingLinkIn ||
       st == State::ConfigError)
      return;
   dev.OpImpl_Reopen();
}

void Device::OpThr_Close(Device& dev, std::string cause) {
   if (dev.State_ < State::Closing)
      dev.OpImpl_Close(std::move(cause));
}
void Device::OpThr_CheckSendEmpty(Device& dev, std::string cause) {
   if (dev.State_ == State::Lingering && dev.IsSendBufferEmpty())
      dev.OpImpl_Close(std::move(cause));
}
void Device::OpThr_LingerClose(Device& dev, std::string cause) {
   if (dev.State_ == State::LinkReady) {
      dev.OpImpl_SetState(State::Lingering, &cause);
      OpThr_CheckSendEmpty(dev, std::move(cause));
   }
   else {
      if (dev.State_ < State::Lingering)
         dev.OpImpl_Close(std::move(cause));
   }
}

void Device::OpImpl_Dispose(std::string cause) {
   (void)cause;
}
void Device::OpThr_Dispose(Device& dev, std::string cause) {
   OpThr_Close(dev, cause);
   OpThr_DisposeNoClose(dev, std::move(cause));
}
void Device::OpThr_DisposeNoClose(Device& dev, std::string cause) {
   if (dev.OpImpl_SetState(State::Disposing, &cause))
      dev.OpImpl_Dispose(std::move(cause));
}

void Device::OpImpl_StartRecv(RecvBufferSize preallocSize) {
   (void)preallocSize;
}
void Device::OpThr_SetLinkReady(Device& dev, std::string stmsg) {
   if (!dev.OpImpl_SetState(State::LinkReady, &stmsg))
      return;
   // 避免在處理 OnDevice_StateChanged() 之後 State_ 被改變(例:特殊情況下關閉了 Device),
   // 所以在此再判斷一次 State_, 必需仍為 LinkReady, 才需觸發 OnDevice_LinkReady() 事件.
   if (dev.State_ == State::LinkReady) {
      dev.CommonTimer_.StopAndWait();
      RecvBufferSize preallocSize = dev.Session_->OnDevice_LinkReady(dev);
      if (preallocSize == RecvBufferSize::NoRecvEvent)
         dev.Options_.Flags_ |= DeviceFlag::NoRecvEvent;
      else
         dev.Options_.Flags_ -= DeviceFlag::NoRecvEvent;
      dev.OpImpl_StartRecv(preallocSize);
   }
}
void Device::OpThr_SetBrokenState(Device& dev, std::string cause) {
   switch (dev.State_) {
   case State::Initializing:
   case State::Initialized:
   case State::ConfigError:
      //if (this->State_ < State::Opening   //尚未開啟
   case State::Closing:
   case State::Closed:
   case State::Disposing:
   case State::Destructing:
      // || State::Closing <= this->State_) //已經關閉(or 關閉中)
      // 不理會 Broken 狀態.
   case State::LinkError:
   case State::LinkBroken:
   case State::ListenBroken:
      // 已經是 Broken or Error, 不處理 Broken 狀態.
      return;

   case State::Lingering:
      OpThr_Close(dev, std::move(cause));
      break;
   case State::Opening:
   case State::WaitingLinkIn:
   case State::Linking:
      //if (State::Opening <= this->State_ && this->State_ <= State::Linking)
      dev.OpImpl_SetState(State::LinkError, &cause);
      break;
   case State::LinkReady:
      dev.OpImpl_SetState(State::LinkBroken, &cause);
      break;
   case State::Listening:
      dev.OpImpl_SetState(State::ListenBroken, &cause);
      break;
   }
}

void Device::OpImpl_StateChanged(const StateChangedArgs& e) {
   (void)e;
}
bool Device::OpImpl_SetState(State afst, StrView stmsg) {
   State bfst = this->State_;
   if (bfst == afst) {
      if (afst < State::Disposing) { // Disposing 訊息不需要重複更新.
         StateUpdatedArgs e{afst, stmsg, this->DeviceId_};
         this->Session_->OnDevice_StateUpdated(*this, e);
         if (this->Manager_)
            this->Manager_->OnDevice_StateUpdated(*this, e);
      }
      return false;
   }
   if (bfst >= State::Disposing && afst < bfst)
      // 狀態 >= State::Disposing 之後, 就不可再回頭!
      return false;
   this->State_ = afst;
   StateChangedArgs e{stmsg, this->DeviceId_};
   e.BeforeState_ = bfst;
   e.After_.State_ = afst;
   this->OpImpl_StateChanged(e);
   this->Session_->OnDevice_StateChanged(*this, e);
   if (this->Manager_)
      this->Manager_->OnDevice_StateChanged(*this, e);
   if (afst == State::LinkBroken || afst == State::ListenBroken) {
      if (this->Options_.LinkBrokenReopenInterval_ > 0)
         this->CommonTimer_.RunAfter(TimeInterval_Millisecond(this->Options_.LinkBrokenReopenInterval_));
   }
   else if (afst == State::LinkError) {
      if (this->Options_.LinkErrorRetryInterval_ > 0)
         this->CommonTimer_.RunAfter(TimeInterval_Millisecond(this->Options_.LinkErrorRetryInterval_));
   }
   return true;
}

std::string Device::WaitGetDeviceId() {
   std::string res;
   this->OpQueue_.InplaceOrWait(AQueueTaskKind::Get,
                                DeviceAsyncOp{[&res](Device& dev) {
      res = dev.DeviceId_;
   }});
   return res;
}

void Device::OpImpl_AppendDeviceInfo(std::string& info) {
   (void)info;
}
std::string Device::WaitGetDeviceInfo() {
   std::string res;
   res.reserve(300);
   res.append("|tm=");
   this->OpQueue_.InplaceOrWait(AQueueTaskKind::Get,
                                DeviceAsyncOp{[&res](Device& dev) {
      if (const char* tmstr = ToStrRev_Full(UtcNow()))
         res.append(tmstr, kDateTimeStrWidth);
      res.append("|st=");
      GetStateStr(dev.State_).AppendTo(res);
      res.append("|id={");
      res.append(dev.DeviceId_);
      res.append("}|info=");
      dev.OpImpl_AppendDeviceInfo(res);
   }});
   return res;
}

std::string Device::WaitSetProperty(StrView strTagValueList) {
   struct Parser : public ConfigParser {
      fon9_NON_COPY_NON_MOVE(Parser);
      Device&        Device_;
      CountDownLatch Waiter_{1};
      RevBufferList  RBuf_{128};
      Parser(Device& dev) : Device_(dev) {}
      Result OnTagValue(StrView tag, StrView& value) override {
         return this->Device_.OpImpl_SetProperty(tag, value);
      }
      bool OnErrorBreak(ErrorEventArgs& e) override {
         RevPrint(this->RBuf_, "err=", e, '\n');
         return false;
      }
   };
   Parser pr{*this};
   this->OpQueue_.AddTask(DeviceAsyncOp{[&](Device&) {
      pr.Parse(strTagValueList);
      pr.Waiter_.ForceWakeUp();
   }});
   pr.Waiter_.Wait();
   return BufferTo<std::string>(pr.RBuf_.MoveOut());
}

ConfigParser::Result Device::OpImpl_SetProperty(StrView tag, StrView& value) {
   return this->Options_.OnTagValue(tag, value);
}

std::string Device::OnDevice_Command(StrView cmd, StrView param) {
   if (cmd == "open")
      this->AsyncOpen(param.ToString());
   else if (cmd == "close")
      this->AsyncClose(param.ToString("DeviceCommand.close:"));
   else if (cmd == "lclose")
      this->AsyncLingerClose(param.ToString("DeviceCommand.lclose:"));
   else if (cmd == "dispose")
      this->AsyncDispose(param.ToString("DeviceCommand.dispose:"));
   else if (cmd == "info")
      return this->WaitGetDeviceInfo();
   else if (cmd == "set")
      return this->WaitSetProperty(param);
   else if (cmd == "ses")
      return this->Session_->SessionCommand(*this, param);
   else return std::string{"unknown device command"};
   return std::string();
}
std::string Device::DeviceCommand(StrView cmdln) {
   StrView cmd = StrFetchTrim(cmdln, &isspace);
   return this->OnDevice_Command(cmd, StrTrim(&cmdln));
}

//--------------------------------------------------------------------------//

enum class StateTimerAct {
   Ignore,
   Reopen,
   ToDerived,
   ToSession,
};
static StateTimerAct GetStateTimerAct(State st) {
   switch (st) {
   case State::Initializing:
   case State::Initialized:
   case State::ConfigError:
   case State::Disposing:
   case State::Destructing:
      return StateTimerAct::Ignore;

   case State::LinkError:
   case State::LinkBroken:
   case State::ListenBroken:
      return StateTimerAct::Reopen;

   case State::LinkReady:
      return StateTimerAct::ToSession;

   default:
   case State::Lingering:
   case State::Opening:
   case State::WaitingLinkIn:
   case State::Linking:
   case State::Listening:
   case State::Closing:
   case State::Closed:
      return StateTimerAct::ToDerived;
   }
}
void Device::EmitOnCommonTimer(TimerEntry* timer, TimeStamp now) {
   Device& rthis = ContainerOf(*static_cast<CommonTimer*>(timer), &Device::CommonTimer_);
   switch (GetStateTimerAct(rthis.State_)) {
   default:
   case StateTimerAct::Ignore:
      break;
   case StateTimerAct::Reopen:
      rthis.OpQueue_.AddTask(DeviceAsyncOp{[](Device& dev) {
         if(GetStateTimerAct(dev.State_) == StateTimerAct::Reopen)
            dev.OpImpl_Reopen();
      }});
      break;
   case StateTimerAct::ToDerived:
      rthis.OnCommonTimer(now);
      break;
   case StateTimerAct::ToSession:
      rthis.Session_->OnDevice_CommonTimer(rthis, now);
      break;
   }
}
void Device::OnCommonTimer(TimeStamp now) {
   (void)now;
}

} } // namespaces