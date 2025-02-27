// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/trusted_plugin_channel.h"

#include "base/callback_helpers.h"
#include "components/nacl/common/nacl_renderer_messages.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"

namespace nacl {

TrustedPluginChannel::TrustedPluginChannel(
    NexeLoadManager* nexe_load_manager,
    const IPC::ChannelHandle& handle,
    base::WaitableEvent* shutdown_event,
    bool is_helper_nexe)
    : nexe_load_manager_(nexe_load_manager),
      is_helper_nexe_(is_helper_nexe) {
  channel_ = IPC::SyncChannel::Create(
      handle, IPC::Channel::MODE_CLIENT, this,
      content::RenderThread::Get()->GetIOMessageLoopProxy(), true,
      shutdown_event);
}

TrustedPluginChannel::~TrustedPluginChannel() {
}

bool TrustedPluginChannel::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool TrustedPluginChannel::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TrustedPluginChannel, msg)
    IPC_MESSAGE_HANDLER(NaClRendererMsg_ReportExitStatus, OnReportExitStatus);
    IPC_MESSAGE_HANDLER(NaClRendererMsg_ReportLoadStatus, OnReportLoadStatus);
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TrustedPluginChannel::OnChannelError() {
  if (!is_helper_nexe_)
    nexe_load_manager_->NexeDidCrash();
}

void TrustedPluginChannel::OnReportExitStatus(int exit_status) {
  if (!is_helper_nexe_)
    nexe_load_manager_->set_exit_status(exit_status);
}

void TrustedPluginChannel::OnReportLoadStatus(NaClErrorCode load_status) {
  if (load_status < 0 || load_status > NACL_ERROR_CODE_MAX) {
    load_status = LOAD_STATUS_UNKNOWN;
  }
  // For now, we only report UMA for non-helper nexes
  // (don't report for the PNaCl translators nexes).
  if (!is_helper_nexe_) {
    HistogramEnumerate("NaCl.LoadStatus.SelLdr", load_status,
                       NACL_ERROR_CODE_MAX);
    // Gather data to see if being installed changes load outcomes.
    const char* name = nexe_load_manager_->is_installed()
                           ? "NaCl.LoadStatus.SelLdr.InstalledApp"
                           : "NaCl.LoadStatus.SelLdr.NotInstalledApp";
    HistogramEnumerate(name, load_status, NACL_ERROR_CODE_MAX);
  }
  if (load_status != LOAD_OK) {
    nexe_load_manager_->ReportLoadError(PP_NACL_ERROR_SEL_LDR_START_STATUS,
                                        NaClErrorString(load_status));
  }
}

}  // namespace nacl
