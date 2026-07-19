//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// HostApplication — subclasses the VST3 SDK HostApplication helper to return
// our own host name and to act as the central host context object.
//-----------------------------------------------------------------------------
#pragma once

#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/pluginterfacesupport.h"

#include <atomic>
#include <functional>
#include <memory>

namespace nst3 {

// Forward decl
class ComponentHandler;

// NstPlugInterfaceSupport subclasses the SDK's PlugInterfaceSupport helper
// and advertises exactly the host interfaces nst3 implements. This is the
// list plugins see when they query IHostApplication::queryInterface for
// IPlugInterfaceSupport and call isPlugInterfaceSupported on the result.
//
// We deliberately do NOT advertise GUI-only interfaces (IPlugFrame,
// IPlugView, IPlugViewContentScaleSupport, IContextMenu) since nst3 does
// not implement plugin editor embedding. Advertising them would cause
// plugins to attempt GUI calls that the host cannot fulfill.
class NstPlugInterfaceSupport : public Steinberg::Vst::PlugInterfaceSupport {
public:
    NstPlugInterfaceSupport();
    ~NstPlugInterfaceSupport() noexcept override;
};

// NstHostApplication subclasses Steinberg::Vst::HostApplication and exposes
// hooks for the host to receive callbacks from plugins (e.g. restartComponent).
//
// Note: the SDK's HostApplication base class creates a default
// PlugInterfaceSupport in its constructor and does not expose a setter, so
// NstHostApplication uses the SDK default rather than installing a curated
// NstPlugInterfaceSupport. The default advertises the standard non-GUI VST3
// interfaces and does not advertise GUI-only interfaces, which matches nst3's
// no-editor-embedding scope.
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
