﻿/// \file fon9/io/win/IocpSocket.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_io_win_IocpSocket_hpp__
#define __fon9_io_win_IocpSocket_hpp__
#include "fon9/io/win/IocpService.hpp"
#include "fon9/io/Socket.hpp"
#include "fon9/io/DeviceStartSend.hpp"
#include "fon9/io/DeviceRecvEvent.hpp"

namespace fon9 { namespace io {

fon9_WARN_DISABLE_PADDING;
fon9_MSC_WARN_DISABLE_NO_PUSH(4623); /* 'IocpSendASAP_AuxMem' : default constructor was implicitly defined as deleted */
/// \ingroup io
/// 使用 Overlapped I/O 建議 SNDBUF=0, 可以讓 WSASend(1024*1024*100) 更快返回.
///
/// | SNDBUF |      txBytes  | WSASend() elapsed(ms) |
/// |-------:|--------------:|-----------------------|
/// |  10240 | 1024*1024*100 | 57, 33, 30, 28...     |
/// |      0 |             0 | 33,  3,  3,  3...     |
///
/// - Windows 與 Socket 相關的 Device, 因使用 Overlapped IO, 因此 SNDBUF 預設為 0
///   - 因為不論是 SendASAP(), SendBuffered(), 都必定會先把要送的資料放到 send buffer.
///   - 然後 WSASend() 可能會在返回前就觸發 IocpService 的 IocpDone(SendOverlapped) 事件.
///   - 此時的 IocpDone(SendOverlapped) 事件, 因 send buffer 還在 lock, 所以無法立即處理.
///   - 造成需要 "Async.DeviceContinueSend": DeviceStartSend.hpp: DeviceContinueSend(); 使得效率不佳.
///   - 加上 SNDBUF=0 則會降低「WSASend()..unlock send buffer」之前就觸發「IocpDone(SendOverlapped)事件」的機率.
///
class fon9_API IocpSocket : public IocpHandler {
   fon9_NON_COPY_NON_MOVE(IocpSocket);
   bool DropRecv();

protected:
   WSAOVERLAPPED  SendOverlapped_;
   SendBuffer     SendBuffer_;
   const SocketAddress* SendTo_{nullptr}; // 若有效, 則使用 WSASendTo();

   WSAOVERLAPPED  RecvOverlapped_;
   RecvBuffer     RecvBuffer_;

   DWORD          Eno_{0}; // Eno_ 通常在 OnIocp_Error() 時設定, 在解構時清除 SendBuffer_ 使用.

   virtual void OnIocp_Error(OVERLAPPED* lpOverlapped, DWORD eno) override;
   virtual void OnIocp_Done(OVERLAPPED* lpOverlapped, DWORD bytesTransfered) override;
   /// 返回前必須主動再次呼叫 StartRecv();
   virtual void OnIocpSocket_Received(DcQueueList& rxbuf) = 0;
   virtual void OnIocpSocket_Writable(DWORD bytesTransfered) = 0;
   virtual void OnIocpSocket_Error(OVERLAPPED* lpOverlapped, DWORD eno) = 0;

public:
   const Socket   Socket_;

   IocpSocket(IocpServiceSP iosv, Socket&& so, SocketResult& soRes);
   ~IocpSocket();

   virtual unsigned IocpSocketAddRef() = 0;
   virtual unsigned IocpSocketReleaseRef() = 0;

   StrView GetOverlappedKind(OVERLAPPED* lpOverlapped) const {
      return lpOverlapped == &this->SendOverlapped_ ? StrView{"Send"}
         : lpOverlapped == &this->RecvOverlapped_ ? StrView{"Recv"}
      : StrView{"Unknown"};
   }
   std::string GetErrorMessage(OVERLAPPED* lpOverlapped, DWORD eno) const;

   //--------------------------------------------------------------------------//

   void StartRecv(RecvBufferSize preallocSize);
   void ContinueRecv(RecvBufferSize expectSize) {
      this->StartRecv(expectSize);
   }
   RecvBuffer& GetRecvBuffer() {
      return this->RecvBuffer_;
   }
   struct IocpRecvAux {
      static void ContinueRecv(RecvBuffer& rbuf, RecvBufferSize expectSize, bool isEnableReadable) {
         (void)isEnableReadable;
         ContainerOf(rbuf, &IocpSocket::RecvBuffer_).ContinueRecv(expectSize);
      }
      static void DisableReadableEvent(RecvBuffer&) {
         // IOCP socket 透過 WSARecv() 啟動「一次」readable, 所以不用額外取消 readable.
      }
      static SendDirectResult SendDirect(RecvDirectArgs& e, BufferList&& txbuf);
   };

   //--------------------------------------------------------------------------//

   SendBuffer& GetSendBuffer() {
      return this->SendBuffer_;
   }
   void ContinueToSend(DcQueueList& toSend) {
      this->IocpSocketAddRef();
      this->SendAfterAddRef(toSend);
   }
   Device::SendResult SendAfterAddRef(DcQueueList& dcbuf);

   struct SendASAP_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.IocpSocketAddRef();
         sc.Destroy();
         toSend.Append(this->Src_, this->Size_);
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendASAP_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.IocpSocketAddRef();
         Device& dev = sc.GetDevice();
         sc.Destroy();
         toSend.push_back(std::move(*this->Src_));
         if (fon9_LIKELY(!toSend.empty()))
            return impl.SendAfterAddRef(toSend);
         // 會來到此處, 必定: 原本的 this->Src_ 都是 BufferNodeVirtual, 且已經 PopConsumed();
         // 如果此時 impl.SendBuffer_ 為空, 就不需要送出了!
         sc.Create(dev, AQueueTaskKind::Send);
         if (fon9_LIKELY(sc.GetALocker().IsAllowInplace_)) {
            DcQueueList* qu = impl.SendBuffer_.OpImpl_CheckSendQueue();
            if (fon9_LIKELY(qu == nullptr)) {
               sc.Destroy();
               impl.IocpSocketReleaseRef();
               return Device::SendResult{0};
            }
            assert(qu == &toSend && !qu->empty());
         }
         sc.Destroy();
         return impl.SendAfterAddRef(toSend);
      }
   };

   struct SendBuffered_AuxMem : public SendAuxMem {
      using SendAuxMem::SendAuxMem;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.IocpSocketAddRef();
         sc.Destroy();
         toSend.Append(this->Src_, this->Size_);
         DcQueueList emptyBuffer;
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   struct SendBuffered_AuxBuf : public SendAuxBuf {
      using SendAuxBuf::SendAuxBuf;

      Device::SendResult StartToSend(DeviceOpLocker& sc, DcQueueList& toSend) {
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.IocpSocketAddRef();
         // 來到此處 dev.SendBuffer_.Status_ 必定在 Sending 狀態,
         // 在 Sending 狀態的 SendBuffer, 會將送出要求放在另一個 Queue_ (不會是這裡的 toSend),
         // 所以這裡可以先將 sc 銷毀(解鎖), 並安全的把 this->Src_ 放到 toSend;
         sc.Destroy();
         toSend.push_back(std::move(*this->Src_));
         // 用 emptyBuffer 讓 SendAfterAddRef() 啟動 Writable 事件,
         // 然後在 Writable 事件裡面送出 toSend;
         DcQueueList emptyBuffer;
         return impl.SendAfterAddRef(emptyBuffer);
      }
   };

   //--------------------------------------------------------------------------//

   struct ContinueSendAux {
      DWORD BytesTransfered_;
      ContinueSendAux(DWORD bytesTransfered) : BytesTransfered_(bytesTransfered) {
      }
      DcQueueList* GetContinueToSend(SendBuffer& sbuf) const {
         return sbuf.OpImpl_ContinueSend(this->BytesTransfered_);
      }
      static void DisableWritableEvent(SendBuffer&) {
         // IOCP socket 透過 WSASend() 啟動「一次」writable, 所以不用額外取消 writable.
      }
      void ContinueToSend(ContinueSendChecker& sc, DcQueueList& toSend) const {
         sc.Destroy();
         IocpSocket& impl = ContainerOf(SendBuffer::StaticCast(toSend), &IocpSocket::SendBuffer_);
         impl.ContinueToSend(toSend);
      }
   };
};
fon9_WARN_POP;

} } // namespaces
#endif//__fon9_io_win_IocpSocket_hpp__
