// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/application_context_impl.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/time/default_tick_clock.h"
#include "base/tracked_objects.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_service.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_time/network_time_tracker.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_query_params.h"
#include "components/variations/service/variations_service.h"
#include "components/web_resource/promo_resource_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/component_updater/ios_component_updater_configurator.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/ios_chrome_io_thread.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_services_manager_client.h"
#include "ios/chrome/browser/net/crl_set_fetcher.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/prefs/ios_chrome_pref_service_factory.h"
#include "ios/chrome/browser/safe_browsing/safe_browsing_service.h"
#include "ios/chrome/browser/update_client/ios_chrome_update_query_params_delegate.h"
#include "ios/chrome/browser/web_resource/web_resource_util.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "net/log/net_log_capture_mode.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// Dummy flag because iOS does not support disabling background networking.
extern const char kDummyDisableBackgroundNetworking[] =
    "dummy-disable-background-networking";
}

ApplicationContextImpl::ApplicationContextImpl(
    base::SequencedTaskRunner* local_state_task_runner,
    const base::CommandLine& command_line,
    const std::string& locale)
    : local_state_task_runner_(local_state_task_runner),
      was_last_shutdown_clean_(false),
      created_local_state_(false) {
  DCHECK(!GetApplicationContext());
  SetApplicationContext(this);

  net_log_.reset(new net_log::ChromeNetLog(
      base::FilePath(), net::NetLogCaptureMode::Default(),
      command_line.GetCommandLineString(), GetChannelString()));

  SetApplicationLocale(locale);

  update_client::UpdateQueryParams::SetDelegate(
      IOSChromeUpdateQueryParamsDelegate::GetInstance());
}

ApplicationContextImpl::~ApplicationContextImpl() {
  DCHECK_EQ(this, GetApplicationContext());
  tracked_objects::ThreadData::EnsureCleanupWasCalled(4);
  SetApplicationContext(nullptr);
}

// static
void ApplicationContextImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kApplicationLocale, std::string());
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
  registry->RegisterBooleanPref(metrics::prefs::kMetricsReportingEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kLastSessionExitedCleanly, true);
  registry->RegisterBooleanPref(prefs::kMetricsReportingWifiOnly, true);
  registry->RegisterBooleanPref(prefs::kLastSessionUsedWKWebViewControlGroup,
                                false);
}

void ApplicationContextImpl::PreCreateThreads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ios_chrome_io_thread_.reset(
      new IOSChromeIOThread(GetLocalState(), GetNetLog()));
}

void ApplicationContextImpl::PreMainMessageLoopRun() {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableIOSWebResources)) {
    DCHECK(!promo_resource_service_.get());
    promo_resource_service_.reset(new web_resource::PromoResourceService(
        GetLocalState(), ::GetChannel(), GetApplicationLocale(),
        GetSystemURLRequestContext(), kDummyDisableBackgroundNetworking,
        web_resource::GetIOSChromeParseJSONCallback()));
    promo_resource_service_->StartAfterDelay();
  }
}

void ApplicationContextImpl::StartTearDown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We need to destroy the MetricsServicesManager, PromoResourceService and
  // SafeBrowsing before the IO thread gets destroyed, since their destructors
  // can call the URLFetcher destructor, which does a PostDelayedTask operation
  // on the IO thread. (The IO thread will handle that URLFetcher operation
  // before going away.)
  if (safe_browsing_service_)
    safe_browsing_service_->ShutDown();

  metrics_services_manager_.reset();

  // Need to clear browser states before the IO thread.
  chrome_browser_state_manager_.reset();

  // PromoResourceService must be destroyed after the keyed services and before
  // the IO thread.
  promo_resource_service_.reset();

  // The GCMDriver must shut down while the IO thread is still alive.
  if (gcm_driver_)
    gcm_driver_->Shutdown();

  if (local_state_) {
    local_state_->CommitPendingWrite();
  }
}

void ApplicationContextImpl::PostDestroyThreads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Resets associated state right after actual thread is stopped as
  // IOSChromeIOThread::Globals cleanup happens in CleanUp on the IO
  // thread, i.e. as the thread exits its message loop.
  //
  // This is important because in various places, the IOSChromeIOThread
  // object being NULL is considered synonymous with the IO thread
  // having stopped.
  ios_chrome_io_thread_.reset();
}

void ApplicationContextImpl::OnAppEnterForeground() {
  DCHECK(thread_checker_.CalledOnValidThread());

  PrefService* local_state = GetLocalState();
  local_state->SetBoolean(prefs::kLastSessionExitedCleanly, false);

  // Tell the metrics service that the application resumes.
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service && local_state) {
    metrics_service->OnAppEnterForeground();
    local_state->CommitPendingWrite();
  }

  variations::VariationsService* variations_service = GetVariationsService();
  if (variations_service)
    variations_service->OnAppEnterForeground();
}

void ApplicationContextImpl::OnAppEnterBackground() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Mark all the ChromeBrowserStates as clean and persist history.
  std::vector<ios::ChromeBrowserState*> loaded_browser_state =
      GetChromeBrowserStateManager()->GetLoadedBrowserStates();
  for (ios::ChromeBrowserState* browser_state : loaded_browser_state) {
    if (history::HistoryService* history_service =
            ios::HistoryServiceFactory::GetForBrowserStateIfExists(
                browser_state, ServiceAccessType::EXPLICIT_ACCESS)) {
      history_service->HandleBackgrounding();
    }

    PrefService* browser_state_prefs = browser_state->GetPrefs();
    if (browser_state_prefs)
      browser_state_prefs->CommitPendingWrite();
  }

  PrefService* local_state = GetLocalState();
  local_state->SetBoolean(prefs::kLastSessionExitedCleanly, true);

  // Tell the metrics service it was cleanly shutdown.
  metrics::MetricsService* metrics_service = GetMetricsService();
  if (metrics_service && local_state)
    metrics_service->OnAppEnterBackground();

  // Persisting to disk is protected by a critical task, so no other special
  // handling is necessary on iOS.
}

bool ApplicationContextImpl::WasLastShutdownClean() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Make sure the locale state is created as the file is initialized there.
  ignore_result(GetLocalState());
  return was_last_shutdown_clean_;
}

PrefService* ApplicationContextImpl::GetLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!created_local_state_)
    CreateLocalState();
  return local_state_.get();
}

net::URLRequestContextGetter*
ApplicationContextImpl::GetSystemURLRequestContext() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ios_chrome_io_thread_->system_url_request_context_getter();
}

const std::string& ApplicationContextImpl::GetApplicationLocale() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!application_locale_.empty());
  return application_locale_;
}

ios::ChromeBrowserStateManager*
ApplicationContextImpl::GetChromeBrowserStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!chrome_browser_state_manager_) {
    chrome_browser_state_manager_ =
        ios::GetChromeBrowserProvider()->CreateChromeBrowserStateManager();
    DCHECK(chrome_browser_state_manager_.get());
  }
  return chrome_browser_state_manager_.get();
}

metrics_services_manager::MetricsServicesManager*
ApplicationContextImpl::GetMetricsServicesManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_services_manager_) {
    metrics_services_manager_.reset(
        new metrics_services_manager::MetricsServicesManager(make_scoped_ptr(
            new IOSChromeMetricsServicesManagerClient(GetLocalState()))));
  }
  return metrics_services_manager_.get();
}

metrics::MetricsService* ApplicationContextImpl::GetMetricsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetMetricsService();
}

variations::VariationsService* ApplicationContextImpl::GetVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetVariationsService();
}

rappor::RapporService* ApplicationContextImpl::GetRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetMetricsServicesManager()->GetRapporService();
}

net_log::ChromeNetLog* ApplicationContextImpl::GetNetLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return net_log_.get();
}

network_time::NetworkTimeTracker*
ApplicationContextImpl::GetNetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!network_time_tracker_) {
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        make_scoped_ptr(new base::DefaultTickClock), GetLocalState()));
  }
  return network_time_tracker_.get();
}

IOSChromeIOThread* ApplicationContextImpl::GetIOSChromeIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(ios_chrome_io_thread_.get());
  return ios_chrome_io_thread_.get();
}

gcm::GCMDriver* ApplicationContextImpl::GetGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!gcm_driver_)
    CreateGCMDriver();
  DCHECK(gcm_driver_);
  return gcm_driver_.get();
}

web_resource::PromoResourceService*
ApplicationContextImpl::GetPromoResourceService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return promo_resource_service_.get();
}

component_updater::ComponentUpdateService*
ApplicationContextImpl::GetComponentUpdateService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!component_updater_) {
    // Creating the component updater does not do anything, components need to
    // be registered and Start() needs to be called.
    component_updater_ = component_updater::ComponentUpdateServiceFactory(
        component_updater::MakeIOSComponentUpdaterConfigurator(
            base::CommandLine::ForCurrentProcess(),
            GetSystemURLRequestContext()));
  }
  return component_updater_.get();
}

CRLSetFetcher* ApplicationContextImpl::GetCRLSetFetcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!crl_set_fetcher_) {
    crl_set_fetcher_ = new CRLSetFetcher;
  }
  return crl_set_fetcher_.get();
}

safe_browsing::SafeBrowsingService*
ApplicationContextImpl::GetSafeBrowsingService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!safe_browsing_service_) {
    safe_browsing_service_ =
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService();
    safe_browsing_service_->Initialize();
  }
  return safe_browsing_service_.get();
}

void ApplicationContextImpl::SetApplicationLocale(const std::string& locale) {
  DCHECK(thread_checker_.CalledOnValidThread());
  application_locale_ = locale;
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      application_locale_);
}

void ApplicationContextImpl::CreateLocalState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!created_local_state_ && !local_state_);
  created_local_state_ = true;

  base::FilePath local_state_path;
  CHECK(PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple);

  // Register local state preferences.
  RegisterLocalStatePrefs(pref_registry.get());

  local_state_ = ::CreateLocalState(
      local_state_path, local_state_task_runner_.get(), pref_registry);

  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::max(std::min<int>(net::kDefaultMaxSocketsPerProxyServer, 99),
               net::ClientSocketPoolManager::max_sockets_per_group(
                   net::HttpNetworkSession::NORMAL_SOCKET_POOL)));

  // Register the shutdown state before anything changes it.
  if (local_state_->HasPrefPath(prefs::kLastSessionExitedCleanly)) {
    was_last_shutdown_clean_ =
        local_state_->GetBoolean(prefs::kLastSessionExitedCleanly);
  }
}

void ApplicationContextImpl::CreateGCMDriver() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!gcm_driver_);

  base::FilePath store_path;
  CHECK(PathService::Get(ios::DIR_GLOBAL_GCM_STORE, &store_path));
  base::SequencedWorkerPool* worker_pool = web::WebThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  gcm_driver_ = gcm::CreateGCMDriverDesktop(
      make_scoped_ptr(new gcm::GCMClientFactory), GetLocalState(), store_path,
      GetSystemURLRequestContext(), ::GetChannel(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::UI),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
      blocking_task_runner);
}
