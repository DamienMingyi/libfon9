﻿/// \file fon9/seed/SeedAcl.cpp
/// \author fonwinz@gmail.com
#include "fon9/seed/SeedAcl.hpp"
#include "fon9/seed/FieldMaker.hpp"
#include "fon9/FilePath.hpp"

namespace fon9 { namespace seed {

static Fields MakeAclTreeFields() {
   Fields fields;
   fields.Add(fon9_MakeField(AccessList::value_type, second.Rights_,       "Rights"));
   fields.Add(fon9_MakeField(AccessList::value_type, second.MaxSubrCount_, "MaxSubrCount"));
   return fields;
}
static LayoutSP MakeAclTreeLayout(TabFlag tabFlags, TreeFlag treeFlags) {
   return LayoutSP{new Layout1(fon9_MakeField(AccessList::value_type, first, "Path"),
                               new Tab{Named{"AclRights"}, MakeAclTreeFields(), tabFlags},
                               treeFlags)};
}
fon9_API LayoutSP MakeAclTreeLayout() {
   static LayoutSP aclLayout{MakeAclTreeLayout(TabFlag::NoSapling, TreeFlag{})};
   return aclLayout;
}
fon9_API LayoutSP MakeAclTreeLayoutWritable() {
   static LayoutSP aclLayout{MakeAclTreeLayout(TabFlag::NoSapling | TabFlag::Writable, TreeFlag::AddableRemovable)};
   return aclLayout;
}

fon9_API const char* IsVisitorsTree(StrView seedPath) {
   if (seedPath.size() >= 3) {
      const char* pbeg = seedPath.begin();
      if (pbeg[0] == '/' && pbeg[1] == '.' && pbeg[2] == '.') {
         if (seedPath.size() == 3 || pbeg[3] == '/')
            return pbeg + 3;
      }
   }
   return nullptr;
}

bool AclPathParser::NormalizePath(StrView& path) {
   StrView head{"/"};
   if (IsVisitorsTree(path)) {
      const char* pbeg = path.begin();
      head.Reset(pbeg, pbeg + 3);
      path.SetBegin(head.end());
   }

   FilePath::StrList plist = FilePath::SplitPathList(path);
   if (!path.empty())
      return false;
   this->Path_.reserve(path.size());
   this->Path_.assign(head); // this->Path_ = "/" or "/.."
   if (size_t count = plist.size()) {
      size_t pos = this->Path_.size();
      this->IndexEndPos_.resize(count);
      this->IndexEndPos_[0] = pos;
      if (pos == 3) {
         this->Path_.push_back('/'); // this->Path_ = "/../"
         ++pos;
      }
      this->Path_.append(ToStrView(FilePath::MergePathList(plist)));
      for (size_t index = 1; index < count; ++index) {
         pos += plist[index - 1].size();
         this->IndexEndPos_[index] = pos;
         ++pos; // for skip '/';
      }
   }
   return true;
}

const char* AclPathParser::NormalizePathStr(StrView path) {
   this->Path_.assign(path);
   if (this->NormalizePath(path))
      return nullptr;
   this->IndexEndPos_.clear();
   return path.begin();
}

const AccessControl* AclPathParser::GetAccess(const AccessList& acl) const {
   StrView  path{ToStrView(this->Path_)};
   size_t   index = this->IndexEndPos_.size();
   for(;;) {
      auto ifind = acl.find(path);
      if (ifind != acl.end())
         return &ifind->second;
      if (index <= 0)
         return nullptr;
      path.SetEnd(path.begin() + this->IndexEndPos_[--index]);
   }
}

const AccessControl* AclPathParser::CheckAccess(const AccessList& acl, AccessRight needsRights) const {
   const AccessControl* ac = this->GetAccess(acl);
   if (ac == nullptr
       || ac->Rights_ == AccessRight::None
       || (needsRights != AccessRight::None && !IsEnumContains(ac->Rights_, needsRights)))
      return nullptr;
   return ac;
}
   
fon9_API AclPath AclPathNormalize(StrView seedPath) {
   AclPathParser parser;
   if (parser.NormalizePath(seedPath))
      return std::move(parser.Path_);
   return AclPath{seedPath};
}

} } // namespaces
