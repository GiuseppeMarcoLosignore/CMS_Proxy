#pragma once

#include "IInterfaces.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class EventBus {
public:
    using Callback = std::function<void(const std::string&, const nlohmann::json&, const int)>;

    void subscribe(const std::string& topic, Callback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        subscribers_[topic].push_back(std::move(cb));
    }

    void publish(const std::string& topic, const nlohmann::json& message, int someInt) const {
        std::vector<Callback> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(topic);
            if (it != subscribers_.end()) {
                callbacks = it->second;
            }
        }

        for (const auto& cb : callbacks) {
            std::thread([cb, topic, message, someInt]() {
                cb(topic, message, someInt);
            }).detach();
        }
    }

private:
    mutable std::mutex mutex_;
    mutable std::unordered_map<std::string, std::vector<Callback>> subscribers_;
};
