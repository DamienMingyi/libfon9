﻿/// \file fon9/RevPrint.hpp
/// \author fonwinz@gmail.com
#ifndef __fon9_RevPrint_hpp__
#define __fon9_RevPrint_hpp__
#include "fon9/RevPut.hpp"
#include "fon9/ToStrFmt.hpp"
#include "fon9/buffer/RevBufferList.hpp"

namespace fon9 {

template <class RevBuffer>
inline void RevPrint(RevBuffer& rbuf, StrView str) {
   RevPutMem(rbuf, str.begin(), str.end());
}

template <class RevBufferT>
void RevPrint(RevBufferT& rbuf, StrView str, FmtDef fmt) {
   char* pout = rbuf.AllocPrefix(std::max(static_cast<size_t>(fmt.Width_), str.size()));
   pout = ToStrRev(pout, str, fmt);
   rbuf.SetPrefixUsed(pout);
}

template <class RevBuffer, size_t arysz> inline void RevPrint(RevBuffer& rbuf, const char (&chary)[arysz])             { return RevPutMem(rbuf, chary, arysz - (chary[arysz - 1] == 0)); }
template <class RevBuffer, size_t arysz> inline void RevPrint(RevBuffer& rbuf, const char (&chary)[arysz], FmtDef fmt) { return RevPrint(rbuf, StrView{chary}, fmt); }
template <class RevBuffer, size_t arysz> inline void RevPrint(RevBuffer& rbuf, char (&cstr)[arysz])                    { return RevPrint(rbuf, StrView_eos_or_all(cstr)); }
template <class RevBuffer, size_t arysz> inline void RevPrint(RevBuffer& rbuf, char (&cstr)[arysz], FmtDef fmt)        { return RevPrint(rbuf, StrView_eos_or_all(cstr), fmt); }

template <class RevBuffer> inline void RevPrint(RevBuffer& rbuf, char ch)             { return RevPutChar(rbuf, ch); }
template <class RevBuffer> inline void RevPrint(RevBuffer& rbuf, char ch, FmtDef fmt) { return RevPrint(rbuf, StrView{&ch,1}, fmt); }

template <class RevBuffer, class ValueT>
inline auto RevPrint(RevBuffer& rbuf, ValueT&& value) -> decltype(RevPrint(rbuf, ToStrView(value)), void()) {
   return RevPrint(rbuf, ToStrView(value));
}

template <class RevBuffer, class ValueT>
inline auto RevPrint(RevBuffer& rbuf, ValueT&& value, FmtDef fmt) -> decltype(RevPrint(rbuf, ToStrView(value), fmt)) {
   return RevPrint(rbuf, ToStrView(value), fmt);
}

//--------------------------------------------------------------------------//

template <class RevBuffer, class ValueT>
inline auto RevPrint(RevBuffer& rbuf, ValueT value) -> decltype(ToStrRev(static_cast<char*>(nullptr), value), void()) {
   char* pout = rbuf.AllocPrefix(ToStrMaxWidth(value));
   pout = ToStrRev(pout, value);
   rbuf.SetPrefixUsed(pout);
}

template <class RevBufferT, class ValueT, class FmtT>
inline auto RevPrint(RevBufferT& rbuf, ValueT value, const FmtT& fmt) -> decltype(ToStrRev(static_cast<char*>(nullptr), value, fmt), void()) {
   char* pout = rbuf.AllocPrefix(ToStrMaxWidth(value, fmt));
   pout = ToStrRev(pout, value, fmt);
   rbuf.SetPrefixUsed(pout);
}

//--------------------------------------------------------------------------//

struct StopRevPrint {};

template <class RevBuffer, class T1, class T2>
inline auto RevPrint(RevBuffer& rbuf, T1&& value1, T2&& value2)
-> enable_if_t<!std::is_same<decay_t<T2>, AutoFmt<T1>>::value
   && !std::is_base_of<StopRevPrint, decay_t<T1>>::value> {

   RevPrint(rbuf, std::forward<T2>(value2));
   RevPrint(rbuf, std::forward<T1>(value1));
}

/// \ingroup AlNum
/// 利用 ToStrRev(pout, value) 把 value 轉成字串填入 rbuf.
template <class RevBuffer, class T1, class T2, class... ArgsT>
inline auto RevPrint(RevBuffer& rbuf, T1&& value1, T2&& value2, ArgsT&&... args)
-> enable_if_t<sizeof...(args) != 0
   && !std::is_base_of<StopRevPrint, decay_t<T1>>::value
   && !std::is_same<decay_t<T2>, AutoFmt<T1>>::value> {

   RevPrint(rbuf, std::forward<T2>(value2), std::forward<ArgsT>(args)...);
   RevPrint(rbuf, std::forward<T1>(value1));
}

/// \ingroup AlNum
/// 利用 ToStrRev(pout, value, fmt) 把 value 轉成字串填入 rbuf.
template <class RevBuffer, class T1, class FmtT, class... ArgsT>
inline auto RevPrint(RevBuffer& rbuf, T1&& value1, FmtT&& fmt, ArgsT&&... args)
-> enable_if_t<sizeof...(args) != 0 && std::is_same<decay_t<FmtT>, AutoFmt<T1>>::value, void> {

   RevPrint(rbuf, std::forward<ArgsT>(args)...);
   RevPrint(rbuf, std::forward<T1>(value1), std::forward<FmtT>(fmt));
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// 利用 RevPrint() 把 args 轉成 StrT.
template <class StrT, class... ArgsT>
inline StrT& RevPrintAppendTo(StrT& dst, ArgsT&&... args) {
   RevBufferList rbuf{256};
   RevPrint(rbuf, std::forward<ArgsT>(args)...);
   BufferAppendTo(rbuf.MoveOut(), dst);
   return dst;
}
template <class StrT, class... ArgsT>
inline StrT RevPrintTo(ArgsT&&... args) {
   StrT dst;
   RevPrintAppendTo(dst, std::forward<ArgsT>(args)...);
   return dst;
}

//--------------------------------------------------------------------------//

/// \ingroup AlNum
/// - 當  fmt.IsNull(): 轉呼叫 RevPrint(out, std::forward<T>(value));
/// - 當 !fmt.IsNull(): 轉呼叫 RevPrint(out, std::forward<T>(value), AutoFmt<T>{fmt});
template <class RevBuffer, class T>
inline void FmtRevPrint(StrView fmt, RevBuffer& rbuf, T&& value) {
   if (fmt.IsNull())
      RevPrint(rbuf, std::forward<T>(value));
   else
      RevPrint(rbuf, std::forward<T>(value), AutoFmt<T>{fmt});
}

} // namespace
#endif//__fon9_RevPrint_hpp__
