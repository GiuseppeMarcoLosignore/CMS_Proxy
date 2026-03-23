#pragma once
#include "IInterfaces.hpp"
#include <map>
#include <memory>
#include <string>
#include <vector>

struct LradDestination {
    uint16_t id;             // L'ID che arriva dal pacchetto (es: 1 o 2)
    std::string ip_address;  // L'IP del server TCP di destinazione
    uint16_t port;           // La porta del server TCP
};

class ProxyEngine {
public:
    ProxyEngine(std::shared_ptr<IReceiver> r, 
                std::shared_ptr<IProtocolConverter> c, 
                std::shared_ptr<ISender> s);
    void run();
    std::map<uint16_t, LradDestination> getNetworkConfig();

private:
    std::shared_ptr<IReceiver> receiver_;
    std::shared_ptr<IProtocolConverter> converter_;
    std::shared_ptr<ISender> sender_;
};