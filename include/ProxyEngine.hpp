#pragma once
#include "IEntity.hpp"
#include <memory>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::vector<std::shared_ptr<IEntity>> entities,
                std::vector<std::shared_ptr<IEventHandler>> handlers);

    void run();
    void stop();

private:
    std::vector<std::shared_ptr<IEntity>> entities_;
    std::vector<std::shared_ptr<IEventHandler>> handlers_;
};