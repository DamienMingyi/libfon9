﻿/// \file fon9/io/IoDev_UT.cpp
/// \author fonwinz@gmail.com
#include "fon9/io/SimpleManager.hpp"
#include "fon9/TestTools.hpp"

#ifdef fon9_WINDOWS
#include "fon9/io/win/IocpTcpClient.hpp"
#include "fon9/io/win/IocpTcpServer.hpp"
using IoService = fon9::io::IocpService;
using IoServiceSP = fon9::io::IocpServiceSP;
using TcpClient = fon9::io::IocpTcpClient;
using TcpServer = fon9::io::IocpTcpServer;
#else
#include "fon9/io/FdrTcpClient.hpp"
#include "fon9/io/FdrTcpServer.hpp"
#include "fon9/io/FdrServiceEpoll.hpp"
using IoService = fon9::io::FdrServiceEpoll;
using IoServiceSP = fon9::io::FdrServiceSP;
using TcpClient = fon9::io::FdrTcpClient;
using TcpServer = fon9::io::FdrTcpServer;
#endif

using TimeUS = fon9::Decimal<uint64_t, 3>;
TimeUS GetTimeUS() {
   using namespace std::chrono;
   return TimeUS::Make<3>(static_cast<uint64_t>(duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count()));
}

//--------------------------------------------------------------------------//

fon9_WARN_DISABLE_PADDING;
class PingpongSession : public fon9::io::SessionServer {
   fon9_NON_COPY_NON_MOVE(PingpongSession);
   using base = fon9::io::SessionServer;
   bool IsEchoMode_{true};
   
   virtual fon9::io::RecvBufferSize OnDevice_LinkReady(fon9::io::Device&) override {
      return fon9::io::RecvBufferSize::Default;
   }
   virtual std::string SessionCommand(fon9::io::Device& dev, fon9::StrView cmdln) override {
      // cmdln = "a size" or "b size" or "s string"
      // a = ASAP, b = Buffered
      // size = 1024 or 1024k or 100m ...
      //        k=*1000, m=*1000000
      TimeUS         usElapsed;
      size_t         size;
      fon9::StrView  strSend;
      std::string    retval;
      fon9::io::Device::SendResult res;
__NEXT_CMD:
      switch (int chCmd = fon9::StrTrim(&cmdln).Get1st()) {
      default:
         return retval.empty() ? std::string{"Unknown ses command."} : retval;
      case 'e':
         cmdln.SetBegin(cmdln.begin() + 1);
         this->IsEchoMode_ = (cmdln.Get1st() == -1) ? (!this->IsEchoMode_) : true;
         retval = this->IsEchoMode_ ? "echo on" : "echo off";
         goto __NEXT_CMD;
      case 's':
         size = cmdln.size();
         strSend = "Send";
         this->LastSendTime_ = fon9::UtcNow();
         usElapsed = GetTimeUS();
         res = dev.StrSend(cmdln);
         break;
      case 'a': case 'b':
         cmdln.SetBegin(cmdln.begin() + 1);
         const char* endp;
         size = fon9::StrTo(cmdln, 0u, &endp);
         if (size <= 0)
            return "tx size empty.";
         cmdln.SetBegin(endp);
         switch(fon9::StrTrimHead(&cmdln).Get1st()) {
         case 'm':   size *= 1000;
         case 'k':   size *= 1000;  break;
         }
         std::string msg(size, '\0');
         this->LastSendTime_ = fon9::UtcNow();
         usElapsed = GetTimeUS();
         if (chCmd == 'a') {
            strSend = "SendASAP";
            res = dev.SendASAP(msg.data(), msg.size());
         }
         else {
            strSend = "SendBuffered";
            res = dev.SendBuffered(msg.data(), msg.size());
         }
         break;
      }
      usElapsed = GetTimeUS() - usElapsed;
      fon9_LOG_DEBUG("dev.", strSend, "()"
                     "|res=", res,
                     "|size=", size, fon9::FmtDef{","},
                     "|elapsed=", usElapsed, fon9::FmtDef{","}, "(us)");
      return std::string{};
   }
public:
   // 建構時如果有指定 OnDevice_RecvDirect, 則會優先呼叫 OnDevice_RecvDirect();
   PingpongSession(bool isUseDirectIO) : base{isUseDirectIO ? &OnDevice_RecvDirect : nullptr} {
   }

   static fon9::io::RecvBufferSize OnDevice_RecvDirect(fon9::io::RecvDirectArgs& e) {
      e.SendDirect(e.RecvBuffer_.MoveOut());
      return fon9::io::RecvBufferSize::Default;
   }
   virtual fon9::io::RecvBufferSize OnDevice_Recv(fon9::io::Device& dev, fon9::DcQueueList& rxbuf) override {
      this->LastRecvTime_ = fon9::UtcNow();
      size_t rxsz = fon9::CalcDataSize(rxbuf.cfront());
      this->RecvBytes_ += rxsz;
      ++this->RecvCount_;
      if (this->IsEchoMode_)
         dev.Send(rxbuf.MoveOut());
      else
         rxbuf.MoveOut();
      return fon9::io::RecvBufferSize::Default;
   }

   fon9::io::SessionSP OnDevice_Accepted(fon9::io::DeviceServer&) {
      return this;
   }

   fon9::TimeStamp   LastSendTime_;
   fon9::TimeStamp   LastRecvTime_;
   uint64_t          RecvBytes_{0};
   uint64_t          RecvCount_{0};

   void PrintInfo() {
      if (this->RecvCount_ <= 0)
         return;
      bool isEchoMode = this->IsEchoMode_;
      if (this->IsEchoMode_) {
         this->IsEchoMode_ = false;
         std::this_thread::sleep_for(std::chrono::milliseconds{500});
      }
      fon9::TimeInterval elapsed = (this->LastRecvTime_ - this->LastSendTime_);
      fon9_LOG_INFO("Session info"
                    "|lastRecvTime=", this->LastRecvTime_,
                    "|lastSendTime=", this->LastSendTime_,
                    "|elapsed=", elapsed,
                    "\n|recvBytes=", this->RecvBytes_,
                    "|recvCount=", this->RecvCount_,
                    "|avgBytes=", this->RecvBytes_ / this->RecvCount_,
                    "|throughput=", fon9::Decimal<uint64_t,3>(static_cast<double>(this->RecvBytes_) / (elapsed.To<double>() * 1024 * 1024)),
                    "(MB/s)");
      this->RecvBytes_ = this->RecvCount_ = 0;
      this->IsEchoMode_ = isEchoMode;
   }
};
using PingpongSP = fon9::intrusive_ptr<PingpongSession>;
fon9_WARN_POP;

//--------------------------------------------------------------------------//

int main(int argc, const char** argv) {
   if (argc < 3) {
__USAGE:
      std::cout << R"(
Usage:
    c "TcpClientConfigs" "IoServiceConfigs"
    s "TcpServerConfigs"

e.g.
    c "127.0.0.1:9000|Timeout=30" "ThreadCount=2|Wait=Block|Cpus="
    s "9000|ThreadCount=2|Wait=Block|Cpus="
)"
         << std::endl;
      return 3;
   }

#if defined(_MSC_VER) && defined(_DEBUG)
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
   char chMode;
   bool isUseDirectIO = false;
   switch (chMode = argv[1][0]) {
   default:
      goto __USAGE;
   case 'c': case 's':
      isUseDirectIO = (argv[1][1] == 'd');
      break;
   }

   fon9::AutoPrintTestInfo utinfo("IoDev");

   IoServiceSP iosv;
   if (argc >= 4) {
      fon9::io::IoServiceArgs iosvArgs;
      if (const char* perr = iosvArgs.Parse(fon9::StrView_cstr(argv[3]))) {
         std::cout << "IoServiceArgs.Parse|err@:" << perr << std::endl;
         return 3;
      }
      IoService::MakeResult   err;
      iosv = IoService::MakeService(iosvArgs, "IoTest", err);
      if (!iosv) {
         std::cout << "IoService.MakeService|" << fon9::RevPrintTo<std::string>(err) << std::endl;
         return 3;
      }
   }
   else if (chMode == 'c')
      goto __USAGE;

   fon9::io::ManagerSP mgr{new fon9::io::SimpleManager{}};
   PingpongSP          ses{new PingpongSession{isUseDirectIO}};
   fon9::io::DeviceSP  dev;
   if (chMode == 'c')
      dev.reset(new TcpClient(iosv, ses, mgr));
   else
      dev.reset(new TcpServer(iosv, ses, mgr));

   dev->AsyncOpen(argv[2]);

   char cmdbuf[1024];
   while (fgets(cmdbuf, sizeof(cmdbuf), stdin)) {
      fon9::StrView  cmd{fon9::StrView_cstr(cmdbuf)};
      fon9::StrTrim(&cmd);
      if (cmd.empty()) {
         ses->PrintInfo();
         continue;
      }
      if (cmd == "quit")
         break;
      fon9::StrView cmdln{cmd};
      fon9::StrView c1 = fon9::StrFetchTrim(cmdln, &fon9::isspace);
      if (c1 == "log") {
         if (!cmdln.empty())
            fon9::LogLevel_ = static_cast<fon9::LogLevel>(fon9::StrTo(cmdln, 0u));
         std::cout << "LogLevel=" << fon9::GetLevelStr(fon9::LogLevel_) << std::endl;
         continue;
      }
      auto dres = dev->DeviceCommand(cmd);
      if (!dres.empty())
         std::cout << dres << std::endl;
   }
   dev->AsyncDispose("quit");
   // 等候 AsyncDispose() 執行完畢.
   dev->WaitGetDeviceId();
   // wait all AcceptedClient dispose
   while (mgr->use_count() != 2) // mgr(+1), dev->Manager_(+1)
      std::this_thread::yield();
}
