#pragma once
#include "IInterfaces.hpp"
#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

class BinaryConverter : public IProtocolConverter {
public:
    RawPacket convert(const RawPacket& input) override;

private:
    // Converte un messaggio specifico in host byte order.
    // Restituisce true se la conversione è andata a buon fine.
    bool convert_CS_LRAS_change_configuration_order_INS(RawPacket& packet) const;
};