#include "ProxyEngine.hpp"
#include <iostream>

ProxyEngine::ProxyEngine(std::shared_ptr<IReceiver> r, 
                         std::shared_ptr<IProtocolConverter> c, 
                         std::shared_ptr<ISender> s)
    : receiver_(r), converter_(c), sender_(s) 
{
    // Colleghiamo la logica: quando arriva un pacchetto...
    receiver_->set_callback([this](const RawPacket& input) {
        // 1. Convertiamo il pacchetto (Big-Endian -> Logica interna)
        std::vector<RawPacket> output = converter_->convert(input);
        std::map<uint16_t, LradDestination> config = getNetworkConfig();
        // 2. Se la conversione ha prodotto dati validi, inviamo tramite TCP
        if (!output.empty()) {
            for (const auto& packetToSend : output) {
                auto destinationIt = config.find(packetToSend.destinationLradId);
                if (destinationIt != config.end()) {
                    sender_->send(packetToSend, destinationIt->second.ip_address, destinationIt->second.port);
                } else {
                    std::cerr << "[Engine] LRAD ID non configurato: " << packetToSend.destinationLradId << std::endl;
                }
            }
        }
    });

    

}

void ProxyEngine::run() {
    if (receiver_) {
        std::cout << "[Engine] Avvio del ciclo di ricezione..." << std::endl;
        receiver_->start();
    }
}

std::map<uint16_t, LradDestination> ProxyEngine::getNetworkConfig() {
    // Configurazione statica per esempio
    std::map<uint16_t, LradDestination> config;
    config[1] = {1, "127.0.0.1", 9000}; // LRAD ID 1 -> Port/CC
    config[2] = {2, "127.0.0.1", 9000}; // LRAD ID 2 -> Port/ACS
    return config;
}