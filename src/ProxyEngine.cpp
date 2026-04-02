#include "ProxyEngine.hpp"

ProxyEngine::ProxyEngine(std::vector<std::shared_ptr<IEntity>> entities,
                         std::vector<std::shared_ptr<IEventHandler>> handlers)
    : entities_(std::move(entities)),
      handlers_(std::move(handlers)) {
}

void ProxyEngine::run() {
    for (const auto& handler : handlers_) {
        if (handler) {
            handler->start();
        }
    }

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

    for (const auto& handler : handlers_) {
        if (handler) {
            handler->stop();
        }
    }
}
