#pragma once
#include "IInterfaces.hpp"
#include <memory>
#include <vector>

class ProxyEngine {
public:
    ProxyEngine(std::shared_ptr<IReceiver> r, 
                std::shared_ptr<IProtocolConverter> c, 
                std::shared_ptr<ISender> s);
    void run();

private:
    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;
};