#include "Orchestrator.hpp"

#include "EventBus.hpp"
#include "Topics.hpp"

#include <algorithm>
#include <iostream>
#include <mutex>

Orchestrator::Orchestrator(CmsEntity &cmsEntity, AcsEntity &acsEntity, NavsEntity &navsEntity)
    : cmsEntity_(cmsEntity),
      acsEntity_(acsEntity),
      navsEntity_(navsEntity),
      eventBus_(std::make_shared<EventBus>()) {
    
    // Initialize LRAS
    lrasList_.lras_status = 0;
    lrasList_.lras_mode = 0;

    // Initialize empty LRAD list
    lradList_.clear();
}

void Orchestrator::start() {
    std::cout << "[Orchestrator] Starting..." << std::endl;
    
    subscribeTopics();
    
    std::cout << "[Orchestrator] Started" << std::endl;
}

void Orchestrator::stop() {
    std::cout << "[Orchestrator] Stopping..." << std::endl;
    
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
    
    std::cout << "[Orchestrator] Stopped" << std::endl;
}

void Orchestrator::subscribeTopics() {
    if (!eventBus_) {
        return;
    }

    // Subscribe to relevant topics for state updates
    // This is a placeholder for future topic subscriptions
    std::cout << "[Orchestrator] Topics subscribed" << std::endl;
}

bool Orchestrator::isDataUpdated() const {
    // Check if any LRAD or LRAS data has been updated
    // For now, return true to indicate data is available
    std::lock_guard<std::mutex> lradLock(lradMutex_);
    std::lock_guard<std::mutex> lrasLock(lrasMutex_);
    
    return !lradList_.empty() || lrasList_.lras_status != 0;
}

void Orchestrator::setLradFullStatus(Lrad_full status, std::string name_) {
    std::lock_guard<std::mutex> lock(lradMutex_);
    
    status.name = std::move(name_);
    
    // Check if LRAD with same name already exists
    auto it = std::find_if(
        lradList_.begin(),
        lradList_.end(),
        [&status](const Lrad_full& lrad) {
            return lrad.name == status.name;
        }
    );
    
    if (it != lradList_.end()) {
        *it = status;  // Update existing
    } else {
        lradList_.push_back(status);  // Add new
    }
}

void Orchestrator::setLrasFullStatus(Lras_full status) {
    std::lock_guard<std::mutex> lock(lrasMutex_);
    lrasList_ = status;
}

Lrad_full Orchestrator::getLradFullStatus() const {
    std::lock_guard<std::mutex> lock(lradMutex_);
    
    if (lradList_.empty()) {
        return Lrad_full{};
    }
    
    // Return first LRAD or could implement logic to select specific one
    return lradList_.front();
}

Lras_full Orchestrator::getLrasFullStatus() const {
    std::lock_guard<std::mutex> lock(lrasMutex_);
    return lrasList_;
}
