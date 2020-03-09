﻿/// \file fon9/InnFile.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_InnFile_hpp__
#define __fon9_InnFile_hpp__
#include "fon9/File.hpp"
#include "fon9/buffer/BufferList.hpp"
#include "fon9/buffer/DcQueue.hpp"
#include "fon9/Exception.hpp"

namespace fon9 {

/// \ingroup Inn
/// InnFile 不解釋 InnRoomType 的值, 由使用者自行解釋.
using InnRoomType = byte;
using InnRoomSize = uint32_t;

/// 這裡的定義, 只是讓使用 InnFile 的人, 可以有共同的依據.
enum : InnRoomType {
   /// 被釋放的, 沒有使用的 Room.
   /// InnFile 沒有特別針對此 RoomType 做處理,
   /// 所以依然可以 MakeRoomKey(), MakeNextRoomKey().
   kInnRoomType_Free = 0x7f,

   /// 加上此旗標, 表示 exRoomHeader 的第一個欄位是 NextRoomPos.
   kInnRoomType_HasNextRoomPos_Flag = 0x80,
   /// 使用串列方式管理 Free rooms.
   kInnRoomType_FreeList = kInnRoomType_Free | kInnRoomType_HasNextRoomPos_Flag,

   /// 通常放在第1間房, 記錄此 InnFile 額外的訊息.
   /// e.g. InnDbf 用第1間房記錄 TableName list.
   kInnRoomType_ExFileHeader = kInnRoomType_HasNextRoomPos_Flag | 0x01,
};

fon9_MSC_WARN_DISABLE(4623 /* default constructor was implicitly defined as deleted */);
fon9_DEFINE_EXCEPTION(InnRoomPosError, std::runtime_error);
fon9_DEFINE_EXCEPTION(InnRoomSizeError, std::runtime_error);
class InnFileError : public std::runtime_error {
   using base = std::runtime_error;
public:
   ErrC  ErrCode_;
   InnFileError(ErrC e, const char* what) : base{what}, ErrCode_{e} {
   }
};
fon9_WARN_POP;

/// \ingroup Inn
/// 檔案空間, 採用類似 memory alloc 的管理方式, 分配空間 & 釋放空間.
/// - InnFile 不理會分配出去的空間如何使用, InnFile 僅提供最基本的功能.
///   - 所有操作都 **不是** thread safe.
///   - 所有操作都 **立即** 操作檔案.
/// - 除了 Open() 使用 OpenResult 傳回結果, 其餘操作若有錯誤, 則拋出異常.
/// - 檔案格式:
///   - 所有的數字格式使用 big endian
///   - File Header: 64 bytes.
///     - char[20]      "fon9.inn.0001\n"  // const char kInnHeaderStr[20]; 尾端補 '\0'
///     - SizeT         HeaderSize_;       // 64: 包含 kInnHeaderStr.
///     - SizeT         BlockSize_;        // 每個資料區塊的大小, 必定是 kBlockSizeAlign 的倍數.
///     - byte[]        用 '\0' 補足 HeaderSize_(64) bytes.
///   - Room Header: 9 bytes.
///     - SizeT         block count.
///     - InnRoomType   RoomType
///     - SizeT         data size in this room.
///   - 為什麼不壓縮 SizeT, RoomPosT?
///     - 放在 Room 前端, 必須是固定大小, 否則當使用 Append 方式添加資料時:
///       可能會需要移動已存入的資料, 變得很難處理。
///     - 不壓縮, 可以在取得 SizeT, RoomPosT 時, 檢查其內容是否合理:
///       可以當成簡易的資料核對功能。
///     - 實際浪費的空間有限: 即使有 1M Rooms, 最多可能的浪費也僅有 6..11M。
///       - block count(3) + data size(3) = 6;
///       - 如果每個 Room 額外包含 NextRoomPos: sizeof(RoomPosT) - 假設實際使用 3 bytes = 8 - 3 = 5;
///         加上 block count(3) + data size(3) = 6; 所以為 11。
class fon9_API InnFile {
   fon9_NON_COPYABLE(InnFile);

public:
   InnFile();
   ~InnFile();

   //--------------------------------------------------------------------------//

   const std::string& GetOpenName() const {
      return this->Storage_.GetOpenName();
   }

   /// \copydoc File::Sync();
   void Sync() {
      this->Storage_.Sync();
   }

   //--------------------------------------------------------------------------//

   using SizeT = InnRoomSize;
   using RoomPosT = File::PosType;

   static constexpr FileMode kDefaultFileMode() { return FileMode::Read | FileMode::Write | FileMode::DenyWrite | FileMode::CreatePath; }
   enum : SizeT {
      kInnHeaderSize = 64,
      kRoomHeaderSize = static_cast<SizeT>(sizeof(InnRoomType/*RoomType*/) + sizeof(SizeT/*BlockCount*/) + sizeof(SizeT/*DataSize*/))
   };

   //--------------------------------------------------------------------------//

   struct OpenArgs {
      std::string FileName_;
      SizeT       BlockSize_;
      FileMode    OpenMode_;

      OpenArgs(std::string fileName, SizeT blockSize = 64, FileMode openMode = kDefaultFileMode())
         : FileName_{std::move(fileName)}
         , BlockSize_{blockSize}
         , OpenMode_{openMode} {
      }
   };

   using OpenResult = File::Result;
   /// \retval success 0:無資料 or 新檔案, >0:此檔共使用了多少個 Block.
   //                  若檔案已存在, 則 args.BlockSize_ 儲存 inn file 的設定.
   /// \retval errc::already_connected 重複開啟, 這次開啟操作失敗, 不影響先前的開啟狀態.
   /// \retval errc::bad_message 檔案格式有誤.
   OpenResult Open(OpenArgs& args);

   void Close() {
      this->Storage_.Close();
   }

   //--------------------------------------------------------------------------//

   /// - 如果您擁有一把 RoomKey, 則可以從 Room 取出 or 存入.
   /// - 直接拋棄 RoomKey (例: 擁有 roomkey 的物件死亡時),
   ///   則仍會保留您儲存的內容, 當載入時會還原您的資料.
   class RoomKey {
      fon9_NON_COPYABLE(RoomKey);
   public:
      using NoteT = uint16_t;
      using Note4T = uint32_t;

      RoomKey() {
         ZeroStruct(this->Info_);
      }
      RoomKey(RoomKey&& rhs) : Info_(rhs.Info_) {
         rhs.Info_.RoomPos_ = 0;
      }
      RoomKey& operator=(RoomKey&& rhs) {
         this->Info_ = rhs.Info_;
         rhs.Info_.RoomPos_ = 0;
         return *this;
      }

      RoomPosT GetRoomPos() const {
         return this->Info_.RoomPos_;
      }
      RoomPosT GetNextRoomPos() const {
         return this->Info_.RoomPos_ + this->Info_.RoomSize_ + kRoomHeaderSize;
      }

      InnRoomType GetCurrentRoomType() const {
         return this->Info_.CurrentRoomType_;
      }

      /// 設定下次寫入(Write,Rewrite,Reduce...)時將要更新的 RoomType.
      void SetPendingRoomType(InnRoomType value) {
         this->Info_.PendingRoomType_ = value;
      }
      InnRoomType GetPendingRoomType() const {
         return this->Info_.PendingRoomType_;
      }

      /// 設定使用者自訂的額外註記.
      /// e.g. InnDbf 用來記錄: 寫入此房的資料, 是否需要同時寫入同步?
      /// Note, Note4 各自獨立, 不會相互影響, 不會存入 RoomExHeader.
      void SetNote(NoteT value) {
         this->Info_.Note_ = value;
      }
      NoteT GetNote() const {
         return this->Info_.Note_;
      }
      void SetNote4(Note4T value) {
         this->Info_.Note4_ = value;
      }
      Note4T GetNote4() const {
         return this->Info_.Note4_;
      }

      SizeT GetDataSize() const {
         return this->Info_.DataSize_;
      }
      // 不包含 kRoomHeaderSize(room header) 的實際可用空間.
      SizeT GetRoomSize() const {
         return this->Info_.RoomSize_;
      }
      SizeT GetRemainSize() const {
         return this->Info_.RoomSize_ - this->Info_.DataSize_;
      }

      /// 判斷是否為無效的 RoomKey.
      bool operator!() const {
         return this->Info_.RoomPos_ == 0;
      }
      explicit operator bool() const {
         return this->Info_.RoomPos_ > 0;
      }

   private:
      struct Info {
         RoomPosT    RoomPos_;
         SizeT       RoomSize_;
         SizeT       DataSize_;
         InnRoomType CurrentRoomType_;
         InnRoomType PendingRoomType_;
         NoteT       Note_;
         Note4T      Note4_;
      };
      Info  Info_;
      friend class InnFile;

      RoomKey(const Info& info) : Info_(info) {
      }
   };

   /// 使用 roomPos 建立 RoomKey, 若 roomPos 不符合規則, 則會拋出 InnRoomPosError 異常!
   /// - 若 roomPos==0 則表示 first room.
   /// - 如果 roomPos 已到 inn 的最後, 則 (!retval) == true;
   /// - 若需要讀入 exRoomHeader, 此處並沒有考慮 room's data size 是否滿足, 這部分需要要您自行在返回後判斷:
   ///   `if (!roomKey || roomKey.GetDataSize() < exRoomHeaderSize) { 此房無效; }`
   RoomKey MakeRoomKey(RoomPosT roomPos, void* exRoomHeader, SizeT exRoomHeaderSize);
   RoomKey MakeRoomKey(RoomPosT roomPos) {
      return this->MakeRoomKey(roomPos, nullptr, 0);
   }

   /// 取得 roomKey 的下一個房間, 此時的 roomKey 必須是有效的.
   /// 如果 roomKey 無效, 則會拋出 InnRoomPosError 異常!
   /// \code
   ///   InnFile::RoomKey rkey = inn.MakeRoomKey(0, nullptr, 0);
   ///   while(rkey) {
   ///      inn.Read(rkey, ...);
   ///      rkey = inn.MakeNextRoomKey(rkey, nullptr, 0);
   ///   }
   /// \endcode
   RoomKey MakeNextRoomKey(const RoomKey& roomKey, void* exRoomHeader, SizeT exRoomHeaderSize);
   RoomKey MakeNextRoomKey(const RoomKey& roomKey) {
      return this->MakeNextRoomKey(roomKey, nullptr, 0);
   }

   /// 在檔案尾端新增一個 room.
   RoomKey MakeNewRoom(InnRoomType roomType, SizeT size);

   //--------------------------------------------------------------------------//

   /// 重新分配一個 room (e.g. free room => 重新分配使用).
   /// - 若 room 的空間 < requiredSize, 則會拋出 InnRoomSizeError 異常!
   /// - requiredSize=需要的大小, room 的實際可用大小必定 >= requiredSize.
   /// - requiredSize 在此僅作為檢查 RoomSize 使用.
   RoomKey ReallocRoom(RoomPosT roomPos, InnRoomType roomType, SizeT requiredSize) {
      RoomKey roomKey = this->MakeRoomKey(roomPos, nullptr, 0);
      roomKey.Info_.PendingRoomType_ = roomType;
      this->ClearRoom(roomKey, requiredSize);
      return roomKey;
   }

   /// 歸還一個 room (e.g. 不再使用 => free room), 返回時 roomKey 會變成無效.
   void FreeRoom(RoomKey& roomKey, InnRoomType roomType) {
      roomKey.Info_.PendingRoomType_ = roomType;
      this->ClearRoom(roomKey, 0);
      ZeroStruct(roomKey.Info_);
   }

   //--------------------------------------------------------------------------//

   /// 將 room 儲存的內容, 從 offset 開始取出 size bytes, 放入 buf 尾端.
   /// - offset=0 表示從標準 RoomHead 之後的第一個 byte 開始讀取.
   /// - 若 offset 超過 room DataSize 則, 不會取出任何資料, 且傳回 0.
   /// - 若 offset + size > room DataSize 則會自動縮減取出的資料量.
   /// - 實際取出的資料量, 必定 <= size.
   SizeT Read(const RoomKey& roomKey, SizeT offset, SizeT size, BufferList& buf);
   /// buf 大小最少需要 `size` bytes.
   /// 實際取出的資料量, 必定 <= size.
   SizeT Read(const RoomKey& roomKey, SizeT offset, SizeT size, void* buf) {
      return this->Read(roomKey, offset, buf, size);
   }
   SizeT Read(const RoomKey& roomKey, SizeT offset, void* buf, SizeT size);

   /// 將 room 儲存的全部內容取出, 放入 buf 尾端.
   /// 傳回取出的資料量, 必定等於 roomKey.GetDataSize();
   SizeT ReadAll(const RoomKey& roomKey, BufferList& buf) {
      return this->Read(roomKey, 0, roomKey.GetDataSize(), buf);
   }
   SizeT ReadAll(const RoomKey& roomKey, void* buf, SizeT bufsz);

   /// 將 「buf全部」 覆寫入 room, 成功後返回寫入 room 的資料量 = roomKey.GetDataSize() = buf.CalcSize();
   /// 若 room 的空間不足, 則 room 內容不變, 直接拋出 InnRoomSizeError 異常.
   SizeT Rewrite(RoomKey& roomKey, DcQueue& buf);
   SizeT Rewrite(RoomKey& roomKey, const void* pbufmem, SizeT bufsz) {
      DcQueueFixedMem   buf{pbufmem, bufsz};
      return this->Rewrite(roomKey, buf);
   }

   /// 將 buf 寫入 room, 寫入的資料量由 size 指定(必須 <= buf.CalcSize());
   /// - 拋出 InnRoomSizeError 異常:
   ///   - offset > roomKey.GetRoomSize()
   ///   - offset + size > roomKey.GetRoomSize()
   /// - 拋出 InnFileError(std::errc::invalid_argument) 異常:
   ///   - size > buf.CalcSize()
   /// - 若成功完成:
   ///   - 傳回值必定 = size
   ///   - 若 offset + size > roomKey.GetDataSize()
   ///     - 則新的 DataSize = offset + size
   ///     - 否則 DataSize 不變.
   SizeT Write(RoomKey& roomKey, SizeT offset, SizeT size, DcQueue& buf);
   SizeT Write(RoomKey& roomKey, SizeT offset, const void* pbufmem, SizeT bufsz) {
      DcQueueFixedMem   buf{pbufmem, bufsz};
      return this->Write(roomKey, offset, bufsz, buf);
   }

   /// 縮減資料量(或不變, 僅重寫 exRoomHeader), 但不能增加.
   /// - 重新調整 room DataSize, 並寫入額外的 exRoomHeader 資訊.
   /// - newDataSize 不可大於現在的 roomKey.GetDataSize();
   /// - 通常用在 pRoom 需要 Append 或 [覆寫尾端最後一筆] 時, 發現尾端空間不足,
   ///   此時會先分配一個新的 nRoom 寫入,
   ///   然後呼叫此處更新 pRoom, 此時 exRoomHeader 包含 nRoom 的位置.
   void Reduce(RoomKey& roomKey, SizeT newDataSize, const void* exRoomHeader, SizeT exRoomHeaderSize);

   //--------------------------------------------------------------------------//

private:
   SizeT CalcRoomSize(SizeT blockCount) const {
      return blockCount ? static_cast<SizeT>(this->BlockSize_ * blockCount - kRoomHeaderSize) : 0;
   }
   bool IsGoodRoomPos(RoomPosT pos) const {
      return (pos >= this->HeaderSize_) && ((pos - this->HeaderSize_) % this->BlockSize_) == 0;
   }
   void RoomHeaderToRoomKeyInfo(const byte* roomHeader, RoomKey::Info& roomKeyInfo);
   void CheckRoomPos(RoomPosT pos, const char* exNotOpen, const char* exBadPos) const;
   void UpdateRoomHeader(RoomKey::Info& info, SizeT newsz, const char* exResError, const char* exSizeError);
   RoomPosT CheckReadArgs(const RoomKey& roomKey, SizeT offset, SizeT& size);
   void ClearRoom(RoomKey& roomKey, SizeT requiredSize);

   File  Storage_;
   SizeT BlockSize_{0};
   SizeT HeaderSize_;
   File::SizeType FileSize_;
};

} // namespaces
#endif//__fon9_InnFile_hpp__
