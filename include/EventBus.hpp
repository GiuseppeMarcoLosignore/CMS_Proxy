#pragma once

#include "IEvent.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class EventBus {
public:
    using EventPtr = std::shared_ptr<const IEvent>;
    using Callback = std::function<void(const EventPtr&)>;

    void subscribe(const std::string& topic, Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[topic].push_back(std::move(cb));
    }

    void publish(const EventPtr& event) const {
        if (!event) {
            return;
        }

        std::vector<Callback> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(event->topic());
            if (it != subscribers_.end()) {
                callbacks = it->second;
            }
        }

        for (const auto& cb : callbacks) {
            std::thread([cb, event]() {
                cb(event);
            }).detach();
        }
    }

private:
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::string, std::vector<Callback>> subscribers_;
};
