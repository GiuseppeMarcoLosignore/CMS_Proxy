#include "BinaryConverter.hpp"
#include <cstring>
#include <algorithm>

RawPacket BinaryConverter::convert(const RawPacket& input) {
    // Esempio minimo: validiamo che il pacchetto non sia vuoto
    if (input.data.size() < 4) return {}; 

    // ESEMPIO DI PARSING BIG-ENDIAN:
    // Leggiamo un intero a 32 bit dall'inizio del pacchetto
    uint32_t network_val;
    std::memcpy(&network_val, input.data.data(), sizeof(uint32_t));
    
    // Invertiamo da Network (Big) a Host (Little su Windows)
    uint32_t host_val = ntohl(network_val);

    // LOGICA DI CONVERSIONE (Esempio: invertiamo l'ordine dei byte per test)
    RawPacket output = input;
    // ... qui manipoli output.data come desideri ...

    return output;
}