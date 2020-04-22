﻿// \file fon9/seed/SeedFairy.cpp
// \author fonwinz@gmail.com
#include "fon9/seed/SeedFairy.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/seed/SeedVisitor.hpp"

namespace fon9 { namespace seed {

struct SeedFairy::AclTree : public Tree {
   fon9_NON_COPY_NON_MOVE(AclTree);
   AccessList& Acl_;
   AclTree(AccessList& acl)
      : Tree(MakeAclTreeLayout())
      , Acl_(acl) {
   }

   struct TreeOp : public seed::TreeOp {
      fon9_NON_COPY_NON_MOVE(TreeOp);
      using base = seed::TreeOp;
      TreeOp(AclTree& tree) : base(tree) {
      }
      void GridView(const GridViewRequest& req, FnGridViewOp fnCallback) override {
         GridViewResult res{this->Tree_, req.Tab_};
         AccessList&    acl = static_cast<AclTree*>(&this->Tree_)->Acl_;
         MakeGridView(acl, GetIteratorForGv(acl, req.OrigKey_),
                      req, res, &SimpleMakeRowView<AccessList::iterator>);
         fnCallback(res);
      }
      void Get(StrView strKeyText, FnPodOp fnCallback) override {
         AccessList& acl = static_cast<AclTree*>(&this->Tree_)->Acl_;
         auto        ifind = GetIteratorForPod(acl, strKeyText);
         if (ifind == acl.end())
            fnCallback(PodOpResult{this->Tree_, OpResult::not_found_key, strKeyText}, nullptr);
         else {
            using PodOpAcl = PodOpReadonly<AccessList::value_type>;
            PodOpAcl op{*ifind, this->Tree_, strKeyText};
            fnCallback(op, &op);
         }
      }
   };
   void OnTreeOp(FnTreeOp fnCallback) override {
      TreeOp op{*this};
      fnCallback(TreeOpResult{this, OpResult::no_error}, &op);
   }
};

//--------------------------------------------------------------------------//

SeedFairy::SeedFairy(MaTreeSP root)
   : VisitorsTree_{new MaTree{"STree"}}
   , Root_{std::move(root)} {
   this->VisitorsTree_->Add(new NamedSapling(new AclTree{this->Ac_.Acl_}, "Acl"));
}
SeedFairy::~SeedFairy() {
   this->VisitorsTree_->OnParentSeedClear();
}

OpResult SeedFairy::NormalizeSeedPath(StrView& seed, AclPath& outpath, AccessRight* reRights) const {
   switch (seed.Get1st()) {
   case '/':
      break;
   case '~':
      seed.SetBegin(seed.begin() + 1);
      outpath = this->Ac_.Home_;
      outpath.push_back('/');
      break;
   default:
   case '.':
      outpath = this->CurrPath_;
      outpath.push_back('/');
      break;
   }
   seed.AppendTo(outpath);
   seed = ToStrView(outpath);

   AclPathParser pathParser;
   if (!pathParser.NormalizePath(seed)) {
      *reRights = AccessRight::None;
      return OpResult::path_format_error;
   }
   AccessRight needs = *reRights;
   const auto* ac = pathParser.GetAccess(this->Ac_.Acl_);
   *reRights = (ac ? ac->Rights_ : AccessRight::None);
   OpResult res = (*reRights == AccessRight::None || (needs != AccessRight::None && !IsEnumContains(*reRights, needs)))
      ? OpResult::access_denied : OpResult::no_error;
   outpath = std::move(pathParser.Path_);
   seed = ToStrView(outpath);
   return res;
}
MaTree* SeedFairy::GetRootPath(OpResult& opResult, StrView& seed, AclPath& outpath, AccessRight* reRights) const {
   if ((opResult = this->NormalizeSeedPath(seed, outpath, reRights)) != OpResult::no_error)
      return nullptr;
   if (const char* pbeg = IsVisitorsTree(seed)) {
      seed.SetBegin(pbeg);
      return this->VisitorsTree_.get();
   }
   return this->Root_.get();
}

SeedFairy::Request::Request(SeedVisitor& visitor, StrView cmdln) {
   this->CommandArgs_ = SbrFetchNoTrim(cmdln, ' ');
   StrTrimHead(&cmdln);
   switch (this->CommandArgs_.Get1st()) {
   case '`':
   case '\'':
   case '"':
      // 使用引號, 表示強制使用 seed name, 且從 CurrPath 開始.
   case '/':
   case '.':
   case '~':
      this->SeedName_ = this->CommandArgs_;
      this->Command_ = this->CommandArgs_ = cmdln;
      this->Runner_ = new TicketRunnerCommand(visitor, this->SeedName_, cmdln);
      return;
   }
   this->SeedName_ = cmdln;
   this->Command_ = StrFetchNoTrim(this->CommandArgs_, ',');
   if (this->Command_ == "ss") {
      this->Runner_ = new TicketRunnerWrite{visitor, this->SeedName_, this->CommandArgs_};
      return;
   }
   if (this->Command_ == "ps") {
      if (this->CommandArgs_.empty())
         this->Runner_ = new TicketRunnerRead{visitor, this->SeedName_};
      else {
__ARGS_MUST_EMPTY:
         this->Runner_ = TicketRunnerError::ArgumentsMustBeEmpty(visitor);
      }
      return;
   }
   if (this->Command_ == "rs") {
      if (!this->CommandArgs_.empty())
         goto __ARGS_MUST_EMPTY;
      this->Runner_ = new TicketRunnerRemove(visitor, this->SeedName_);
      return;
   }
   if (this->Command_ == "gv") {
      int16_t  rowCount = 0;
      StrView  args = this->CommandArgs_;
      StrView  startKey = TextBegin();
      if (isdigit(args.Get1st()) || args.Get1st()=='-' || args.Get1st() == '+') {
         const char* pend;
         rowCount = StrTo(args, rowCount, &pend);
         args.SetBegin(pend);
      }
      if (!args.empty()) {
         if (args.Get1st() != ',') {
            this->Runner_ = new TicketRunnerError(visitor, OpResult::bad_command_argument, "Invalid GridView arguments.");
            return;
         }
         args.SetBegin(args.begin() + 1);
         startKey = ParseKeyTextAndTabName(args);
      }
      if (rowCount < 0 && IsTextBegin(startKey))
         startKey = TextEnd();
      this->Runner_ = new TicketRunnerGridView(visitor, this->SeedName_, rowCount, startKey, args);
      return;
   }
   if (this->Command_ == "s") { // subscribe: s,TabName TreePath
      this->Runner_ = new TicketRunnerSubscribe(visitor, this->SeedName_, this->CommandArgs_);
      return;
   }
   if (this->Command_ == "u") { // unsubscribe
      TicketRunnerSubscribe* unsub;
      this->Runner_ = unsub = new TicketRunnerSubscribe{visitor, this->SeedName_};
      if (auto subr = unsub->Subr_.get()) {
         if (subr->GetPath() != unsub->OrigPath_)
            this->Runner_ = new TicketRunnerError(visitor, OpResult::bad_command_argument, "Path no subscribe.");
      }
      else
         this->Runner_ = new TicketRunnerError(visitor, OpResult::not_supported_cmd, "No subscribe.");
      return;
   }
}

//--------------------------------------------------------------------------//

TicketRunner::TicketRunner(SeedVisitor& visitor, StrView seed, AccessRight needsRights)
   : SeedTicket{*visitor.Fairy_, seed, needsRights}
   , SeedSearcher{seed}
   , Visitor_{&visitor} {
}
TicketRunner::TicketRunner(SeedVisitor& visitor, OpResult errn, const StrView& errmsg)
   : SeedTicket{errn, errmsg}
   , SeedSearcher{StrView{}}
   , Visitor_{&visitor} {
}
void TicketRunner::OnError(OpResult res) {
   this->OpResult_ = res;
   this->ErrPos_ += static_cast<size_t>(this->RemainPath_.begin() - this->OrigPath_.begin());
   this->Visitor_->OnTicketRunnerDone(*this, DcQueueFixedMem{});
}
void TicketRunner::OnLastStep(TreeOp& op, StrView keyText, Tab& tab) {
   this->RemainPath_.SetBegin(keyText.begin());
   op.Get(keyText, std::bind(&TicketRunner::OnLastSeedOp,
                             intrusive_ptr<TicketRunner>(this),
                             std::placeholders::_1, std::placeholders::_2, std::ref(tab)));
}
void TicketRunner::Run() {
   if (this->Root_)
      StartSeedSearch(*this->Root_, this);
   else
      this->OnError(this->OpResult_ == OpResult::no_error
                    ? OpResult::not_found_sapling
                    : this->OpResult_);
}

} } // namespaces
