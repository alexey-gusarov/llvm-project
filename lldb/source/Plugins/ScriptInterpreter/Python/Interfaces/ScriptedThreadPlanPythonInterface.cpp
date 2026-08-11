//===-- ScriptedThreadPlanPythonInterface.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../lldb-python.h"

#include "lldb/Core/PluginManager.h"
#include "lldb/Target/ThreadPlan.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/StreamString.h"
#include "lldb/lldb-enumerations.h"

#include "../SWIGPythonBridge.h"
#include "../ScriptInterpreterPythonImpl.h"
#include "ScriptedThreadPlanPythonInterface.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::python;

ScriptedThreadPlanPythonInterface::ScriptedThreadPlanPythonInterface(
    ScriptInterpreterPythonImpl &interpreter)
    : ScriptedThreadPlanInterface(), ScriptedPythonInterface(interpreter) {}

llvm::Expected<StructuredData::GenericSP>
ScriptedThreadPlanPythonInterface::CreatePluginObject(
    const ScriptedMetadata &scripted_metadata,
    lldb::ThreadPlanSP thread_plan_sp) {
  StructuredDataImpl args_sp(scripted_metadata.GetArgsSP());
  return ScriptedPythonInterface::CreatePluginObject(scripted_metadata, nullptr,
                                                     thread_plan_sp, args_sp);
}

llvm::Expected<bool>
ScriptedThreadPlanPythonInterface::ExplainsStop(Event *event) {
  Status error;
  StructuredData::ObjectSP obj = Dispatch("explains_stop", error, event);

  if (!ScriptedInterface::CheckStructuredDataObject(LLVM_PRETTY_FUNCTION, obj,
                                                    error)) {
    if (!obj)
      return false;
    return error.ToError();
  }

  return obj->GetBooleanValue();
}

llvm::Expected<bool>
ScriptedThreadPlanPythonInterface::ShouldStop(Event *event) {
  Status error;
  StructuredData::ObjectSP obj = Dispatch("should_stop", error, event);

  if (!ScriptedInterface::CheckStructuredDataObject(LLVM_PRETTY_FUNCTION, obj,
                                                    error)) {
    if (!obj)
      return false;
    return error.ToError();
  }

  return obj->GetBooleanValue();
}

llvm::Expected<bool> ScriptedThreadPlanPythonInterface::IsStale() {
  Status error;
  StructuredData::ObjectSP obj = Dispatch("is_stale", error);

  if (!ScriptedInterface::CheckStructuredDataObject(LLVM_PRETTY_FUNCTION, obj,
                                                    error)) {
    if (!obj)
      return false;
    return error.ToError();
  }

  return obj->GetBooleanValue();
}

lldb::StateType ScriptedThreadPlanPythonInterface::GetRunState() {
  Status error;
  StructuredData::ObjectSP obj = Dispatch("should_step", error);

  if (!ScriptedInterface::CheckStructuredDataObject(LLVM_PRETTY_FUNCTION, obj,
                                                    error))
    return lldb::eStateStepping;

  // `should_step` answers a question, it does not return a StateType: the
  // documented contract is "Return `True` if you want lldb to instruction step
  // one instruction, or False to continue till the next breakpoint is hit"
  // (lldb/docs/use/tutorials/automating-stepping-logic.md).  A Python bool
  // arrives here as a
  // StructuredData::Boolean, and GetUnsignedIntegerValue() returns its fail
  // value for anything that is not an Integer -- so reading the answer as a
  // StateType discarded it, and a plan returning False single-stepped exactly
  // like one returning True.
  if (StructuredData::Boolean *should_step = obj->GetAsBoolean())
    return should_step->GetValue() ? lldb::eStateStepping : lldb::eStateRunning;

  // Anything else is not an answer to the question that was asked.  Step, as
  // this function has always done when it could not read a reply -- but say so
  // in the log, because an int return used to be reinterpreted as a StateType
  // and is the one thing whose behaviour changes here.
  if (Log *log = GetLog(LLDBLog::Script)) {
    StreamString returned;
    obj->Dump(returned, /*pretty_print=*/false);
    LLDB_LOG(log,
             "{0}: should_step returned {1}, which is not a bool; stepping.",
             LLVM_PRETTY_FUNCTION, returned.GetData());
  }
  return lldb::eStateStepping;
}

llvm::Error
ScriptedThreadPlanPythonInterface::GetStopDescription(lldb::StreamSP &stream) {
  Status error;
  Dispatch("stop_description", error, stream);

  if (error.Fail())
    return error.ToError();

  return llvm::Error::success();
}

void ScriptedThreadPlanPythonInterface::Initialize() {
  const std::vector<llvm::StringRef> ci_usages = {
      "thread step-scripted -C <script-name> [-k key -v value ...]"};
  const std::vector<llvm::StringRef> api_usages = {
      "SBThread.StepUsingScriptedThreadPlan"};
  PluginManager::RegisterPlugin(
      GetPluginNameStatic(),
      llvm::StringRef("Alter thread stepping logic and stop reason"),
      CreateInstance, eScriptedExtensionScriptedThreadPlan,
      eScriptLanguagePython, {ci_usages, api_usages});
}

void ScriptedThreadPlanPythonInterface::Terminate() {
  PluginManager::UnregisterPlugin(CreateInstance);
}
