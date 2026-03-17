#pragma once
#include "IInterfaces.hpp"
#include <nlohmann/json.hpp>
#include <string>
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

using json = nlohmann::json;

class BinaryConverter : public IProtocolConverter {
public:
    RawPacket convert(const RawPacket& input) override;

private:
    // Converte un messaggio specifico in host byte order e lo trasforma in JSON.
    // Restituisce true se la conversione è andata a buon fine.
    bool convert_CS_LRAS_change_configuration_order_INS(RawPacket& packet) const;
    
    // Aggiungi qui le funzioni per altri messaggi
    bool convert_CS_LRAS_status_update(RawPacket& packet) const;
    
    // Estrae i dati dal payload e crea un JSON basato sul messageId
    json extractPayloadToJson(const RawPacket& packet, uint32_t messageId) const;
};