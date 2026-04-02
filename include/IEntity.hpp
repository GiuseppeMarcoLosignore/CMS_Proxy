#pragma once

class IEntity {
public:
    virtual ~IEntity() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class IEventHandler {
public:
    virtual ~IEventHandler() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};
