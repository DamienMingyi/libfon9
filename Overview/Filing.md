﻿libfon9 基礎建設程式庫 - Filing
===============================

## File/Path tools
* [`fon9/File.hpp`](../fon9/File.hpp)
  * 在讀寫時指定位置(由 OS 保證)，避免不同 thread 同時讀寫，造成 seek + rw 之間交錯造成的問題。
  * Append 開檔模式，由 OS 保證寫入時，一定寫到檔案尾端。
* [`fon9/FilePath.hpp`](../fon9/FilePath.hpp)
  * 檔名拆解、組合工具。
  * 建立路徑工具。
* [`fon9/TimedFileName.hpp`](../fon9/TimedFileName.hpp)
  * 由 [時間] 及 [檔案大小(超過指定大小則增加檔名序號)] 決定檔名。

-------------------------------

## Serialize/Deserialize
### Text
### Bitv
[Binary type value encoding protocol](Bitv.md)

-------------------------------

## InnFile
* [`fon9/InnFile.hpp`](../fon9/InnFile.hpp)
* 著重功能：
  * 房間的建立、取得。
  * 讀取房間內的資料。
  * 將資料寫入房間內。
* 進階功能：由 InnDbf 提供。

## InnDbf
* [`fon9/InnDbf.hpp`](../fon9/InnDbf.hpp)
* 透過 `InnDbf::LinkTable()` 將 TableHandler 綁好
* 然後 `InnDbf::LoadAll()` 載入全部的 Row(包含 deleted row) 透過 TableHandler 重建「記憶體中的資料表」。
* InnDbf 若有綁定 Syncer，則在收到同步資料時，
  也會透過 TableHandler「更新、新增、刪除」記憶體中的資料表。

### 資料表的連結
* [`fon9/InnDbfTable.hpp`](../fon9/InnDbfTable.hpp)
* InnDbfTableLink：負責與 InnDbf 溝通。
* InnDbfTableHandler：負責與 使用者的資料表 溝通。

### 資料同步機制
* 前提
  * 一般用於儲存：使用者資料表、使用者政策... 這類不常異動的資料。
    * 行情、技術分析、委託... 之類的資料，不使用這裡的同步機制。
  * 一般情況下同一筆資料，不會同時在不同主機(或同一台主機)上異動。
  * 所以不用達到「交易等級的 [ACID](https://zh.wikipedia.org/wiki/ACID) 特性」。
  * 僅需要達到「最終一致」即可。
  * 即使有衝突，e.g. 真的在同一瞬間，在不同主機上，同時更新使用者密碼。
    * 此時就簡單的用 SyncKey 來決定哪一筆為較新的資料。
* 如何判斷同步資料的新或舊?
  * 每筆資料包含一個 SyncKey: HostId + TimeStamp + Seq
  * 藉由 SyncKey 來判斷，該筆訊息是否需要處理：
    * 該拋棄的老舊過期訊息。
    * 或是必須處理的較新異動訊息。
  * 對於刪除的資料，因為已從「記憶體中的資料表」刪除，所以必須額外記住刪除時的 SyncKey，
    避免收到較舊的異動 (異動的 SyncKey < 刪除的 SyncKey)。
* TODO: 清除「Deleted 紀錄」的時機。

## InnApf
* 使用 InnFile 儲存用 key 索引, 每個 key 可以儲存大量資料。
* 為了解決「技術分析」,「行情交易明細」這類需求:
  * 需要快速 Append 儲存，避免影響行情處理速度。
  * 通常使用循序讀取，隨機讀取需求不高。
* TODO: 刪除 key, 釋放 InnRoom, 重新分配使用。
