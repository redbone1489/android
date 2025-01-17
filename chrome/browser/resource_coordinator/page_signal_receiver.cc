// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/page_signal_receiver.h"

#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace resource_coordinator {

namespace {
base::LazyInstance<PageSignalReceiver>::Leaky g_page_signal_receiver;
}

// static
bool PageSignalReceiver::IsEnabled() {
  // Check that service_manager is active and Resource Coordinator is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr &&
         resource_coordinator::IsResourceCoordinatorEnabled();
}

// static
PageSignalReceiver* PageSignalReceiver::GetInstance() {
  if (!IsEnabled())
    return nullptr;
  return g_page_signal_receiver.Pointer();
}

PageSignalReceiver::PageSignalReceiver() : binding_(this) {}

PageSignalReceiver::~PageSignalReceiver() = default;

void PageSignalReceiver::NotifyPageAlmostIdle(
    const CoordinationUnitID& page_cu_id) {
  DCHECK(IsPageAlmostIdleSignalEnabled());
  NotifyObserversIfKnownCu(page_cu_id, &PageSignalObserver::OnPageAlmostIdle);
}

void PageSignalReceiver::NotifyRendererIsBloated(
    const CoordinationUnitID& page_cu_id) {
  auto web_contents_iter = cu_id_web_contents_map_.find(page_cu_id);
  if (web_contents_iter == cu_id_web_contents_map_.end())
    return;
  for (auto& observer : observers_)
    observer.OnRendererIsBloated(web_contents_iter->second);
}

void PageSignalReceiver::SetExpectedTaskQueueingDuration(
    const CoordinationUnitID& page_cu_id,
    base::TimeDelta duration) {
  NotifyObserversIfKnownCu(
      page_cu_id, &PageSignalObserver::OnExpectedTaskQueueingDurationSet,
      duration);
}

void PageSignalReceiver::SetLifecycleState(const CoordinationUnitID& page_cu_id,
                                           mojom::LifecycleState state) {
  NotifyObserversIfKnownCu(page_cu_id,
                           &PageSignalObserver::OnLifecycleStateChanged, state);
}

void PageSignalReceiver::NotifyNonPersistentNotificationCreated(
    const CoordinationUnitID& page_cu_id) {
  NotifyObserversIfKnownCu(
      page_cu_id, &PageSignalObserver::OnNonPersistentNotificationCreated);
}

void PageSignalReceiver::OnLoadTimePerformanceEstimate(
    const CoordinationUnitID& page_cu_id,
    const std::string& url,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  NotifyObserversIfKnownCu(
      page_cu_id, &PageSignalObserver::OnLoadTimePerformanceEstimate, url,
      cpu_usage_estimate, private_footprint_kb_estimate);
}

void PageSignalReceiver::AddObserver(PageSignalObserver* observer) {
  // When PageSignalReceiver starts to have observer, construct the mojo
  // channel.
  if (!observers_.might_have_observers()) {
    content::ServiceManagerConnection* service_manager_connection =
        content::ServiceManagerConnection::GetForProcess();
    // Ensure service_manager is active before trying to connect to it.
    if (service_manager_connection) {
      service_manager::Connector* connector =
          service_manager_connection->GetConnector();
      mojom::PageSignalGeneratorPtr page_signal_generator_ptr;
      connector->BindInterface(mojom::kServiceName,
                               mojo::MakeRequest(&page_signal_generator_ptr));
      mojom::PageSignalReceiverPtr page_signal_receiver_ptr;
      binding_.Bind(mojo::MakeRequest(&page_signal_receiver_ptr));
      page_signal_generator_ptr->AddReceiver(
          std::move(page_signal_receiver_ptr));
    }
  }
  observers_.AddObserver(observer);
}

void PageSignalReceiver::RemoveObserver(PageSignalObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PageSignalReceiver::AssociateCoordinationUnitIDWithWebContents(
    const CoordinationUnitID& cu_id,
    content::WebContents* web_contents) {
  cu_id_web_contents_map_[cu_id] = web_contents;
}

void PageSignalReceiver::RemoveCoordinationUnitID(
    const CoordinationUnitID& cu_id) {
  cu_id_web_contents_map_.erase(cu_id);
}

}  // namespace resource_coordinator
