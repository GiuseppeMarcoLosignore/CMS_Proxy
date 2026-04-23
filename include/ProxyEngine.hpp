#pragma once
#include "IInterfaces.hpp"
#include <memory>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::vector<std::shared_ptr<IEntity>> entities);

    void run();
    void stop();

private:
    std::vector<std::shared_ptr<IEntity>> entities_;
};