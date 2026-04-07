#include "ProxyEngine.hpp"

ProxyEngine::ProxyEngine(std::vector<std::shared_ptr<IEntity>> entities)
    : entities_(std::move(entities)) {
}

void ProxyEngine::run() {
    for (const auto& entity : entities_) {
        if (entity) {
            entity->start();
        }
    }
}

void ProxyEngine::stop() {
    for (const auto& entity : entities_) {
        if (entity) {
            entity->stop();
        }
    }
}
