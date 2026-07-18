//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// HostApplication — subclasses the VST3 SDK HostApplication helper to return
// our own host name and to act as the central host context object.
//-----------------------------------------------------------------------------
#pragma once

#include "public.sdk/source/vst/hosting/hostclasses.h"

#include <atomic>
#include <functional>
#include <memory>

namespace nst3 {

// Forward decl
class ComponentHandler;

// NstHostApplication subclasses Steinberg::Vst::HostApplication and exposes
// hooks for the host to receive callbacks from plugins (e.g. restartComponent).
class NstHostApplication : public Steinberg::Vst::HostApplication {
public:
    NstHostApplication();
    ~NstHostApplication() noexcept override;

    // IHostApplication
    Steinberg::tresult PLUGIN_API getName(Steinberg::Vst::String128 name) override;

    // Set the component handler that the host uses to route beginEdit / restart
    // calls. Stored as a raw pointer (the handler is owned by PluginInstance).
    void setComponentHandler(ComponentHandler* handler) { handler_ = handler; }

    ComponentHandler* componentHandler() const { return handler_; }

private:
    ComponentHandler* handler_ = nullptr;
};

} // namespace nst3
