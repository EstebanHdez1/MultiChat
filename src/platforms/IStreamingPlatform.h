#pragma once

#include <string>
#include "PlatformStatus.h"

class IStreamingPlatform
{
public:
    virtual ~IStreamingPlatform() = default;

    virtual bool connectPlatform() = 0;
    virtual void disconnectPlatform() = 0;

    virtual bool isOnline() const = 0;
    virtual PlatformStatus getStatus() const = 0;

    virtual void startChatListener() = 0;
    virtual void stopChatListener() = 0;

    virtual int getViewerCount() const = 0;
    virtual std::string getPlatformName() const = 0;

    virtual void setChannel(const std::string& channelName, const std::string& extraKey = "") = 0;
    virtual std::string getChannel() const = 0;
};
