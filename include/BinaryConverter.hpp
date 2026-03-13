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
};