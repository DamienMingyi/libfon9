﻿/// \file fon9/framework/Framework.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_framework_Framework_hpp__
#define __fon9_framework_Framework_hpp__
#include "fon9/seed/PluginsMgr.hpp"
#include "fon9/auth/AuthMgr.hpp"

namespace fon9 {

/// \ingroup Misc
struct fon9_API Framework {
   std::string          ConfigPath_;
   std::string          SyncerPath_;
   seed::MaTreeSP       Root_;
   InnSyncerSP          Syncer_;
   auth::AuthMgrSP      MaAuth_;
   seed::PluginsMgrSP   MaPlugins_;

   ~Framework();

   /// create system default object.
   /// - 設定工作目錄:  `-w dir` or `--workdir dir`
   /// - ConfigPath_ = `-c cfgpath` or `--cfg cfgpath` or `getenv("fon9cfg");` or default="fon9cfg"
   /// - 然後載入設定: fon9local.cfg, fon9common.cfg; 設定內容包含:
   ///   - LogFileFmt  如果沒設定, log 就輸出在 console.
   ///     - $LogFileFmt=./logs/{0:f+'L'}/fon9sys-{1:04}.log  # 超過 {0:f+'L'}=YYYYMMDD(localtime), {1:04}=檔案序號.
   ///     - $LogFileSizeMB=n                                 # 超過 n MB 就換檔.
   ///   - $HostId     沒有預設值, 如果沒設定, 就不會設定 LocalHostId_
   ///   - $SyncerPath 指定 InnSyncerFile 的路徑, 預設 = "fon9syn"
   ///   - $MaAuthName 預設 "MaAuth": 並建立(開啟) this->ConfigPath_ + $MaAuthName + ".f9dbf" 儲存 this->MaAuth_ 之下的資料表.
   ///   - $MemLock    預設 "N"
   ///   - $MaPlugins  預設 "MaPlugins.f9gv": 儲存 「plugins設定」的檔案, 實際儲存位置為: this->ConfigPath_ + $MaPlugins
   void Initialize(int argc, char** argv);

   /// dbf.LoadAll(), syncer.StartSync(), ...
   void Start();

   /// syncer.StopSync(), dbf.Close(), root.OnParentSeedClear(), ...
   void Dispose();

   /// Dispose() 並且等候相關 thread 執行完畢.
   void DisposeForAppQuit();
};

/// 必須將 fon9/framework/Fon9CoRun.cpp 加入您的執行專案裡面, 在 int main() 呼叫 Fon9CoRun();
/// - 在 framework.Start() 之前會呼叫 fnBeforeStart(framework), 若傳回非0, 則立即結束 Fon9CoRun().
/// - 您可以在 fnBeforeStart() 啟動您自訂的物件, 例如:
///   - `f9omstw::OmsPoIvListAgent::Plant(*fon9sys.MaAuth_);`
int Fon9CoRun(int argc, char** argv, int (*fnBeforeStart)(Framework&));

} // namespaces
#endif//__fon9_framework_Framework_hpp__
