#!/bin/bash

# 在指定路徑建立 proj_verinfo.h:
# - Linux 在 CMakeLists.txt 裡面執行:
#    execute_process(COMMAND "../fon9/make_proj_verinfo.sh" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
# - 然後可以在程式裡將版本資放入 SysEnv:
#   if (auto sysEnv = holder.Root_->Get<fon9::seed::SysEnv>(fon9_kCSTR_SysEnv_DefaultName))
#      fon9::seed::LogSysEnv(sysEnv->Add(new fon9::seed::SysEnvItem("Version", proj_VERSION)).get());

pushd $(dirname "$0")
FON9_HASH=`git rev-parse --short=12 HEAD | xargs`
popd

MySln_HASH=`git rev-parse --short=12 HEAD | xargs`

BUILDTM=`date +"%Y.%m.%d-%H%M"`

PROJ=`basename "${PWD}"`

echo "#define proj_NAME    \"${PROJ}\"" > proj_verinfo.h
echo "#define proj_VERSION \"${PROJ}=${MySln_HASH}|fon9=${FON9_HASH}|build=${BUILDTM}\"" >> proj_verinfo.h
