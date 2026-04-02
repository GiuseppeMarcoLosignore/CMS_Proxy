#pragma once

#include <string>

class IEvent {
public:
    virtual ~IEvent() = default;
    virtual const std::string& topic() const = 0;
};
