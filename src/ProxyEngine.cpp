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
        RawPacket output = converter_->convert(input);
        
        // 2. Se la conversione ha prodotto dati validi, inviamo tramite TCP
        if (!output.data.empty()) {
            sender_->send(output);
        }
    });
}

void ProxyEngine::run() {
    if (receiver_) {
        std::cout << "[Engine] Avvio del ciclo di ricezione..." << std::endl;
        receiver_->start();
    }
}