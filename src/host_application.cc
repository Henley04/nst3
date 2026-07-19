//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// HostApplication implementation
//-----------------------------------------------------------------------------
#include "host_application.h"

#include "string_convert.h"

// SDK interface headers — needed for the IXXX::iid FUID constants that we
// advertise via NstPlugInterfaceSupport.
#include "pluginterfaces/vst/ivsteditcontroller.h"       // IComponentHandler{,2,3}, IEditController{,2}, IParameterFinder
#include "pluginterfaces/vst/ivsthostapplication.h"      // IHostApplication
#include "pluginterfaces/vst/ivstmessage.h"              // IMessage, IAttributeList
#include "pluginterfaces/vst/ivstconnectionpoints.h"     // IConnectionPoint
#include "pluginterfaces/vst/ivstunits.h"                // IUnitHandler{,2}
#include "pluginterfaces/vst/ivstpluginterfacesupport.h" // IPlugInterfaceSupport

namespace nst3 {

//------------------------------------------------------------------------
// NstPlugInterfaceSupport
//------------------------------------------------------------------------
// The constructor populates the supported-FUID list with exactly the host
// interfaces nst3 implements. We use the SDK helper's addPlugInterfaceSupported
// so isPlugInterfaceSupported() returns kResultTrue for each advertised FUID.
NstPlugInterfaceSupport::NstPlugInterfaceSupport() {
    // Host-side interfaces implemented by ComponentHandler / NstHostApplication.
    addPlugInterfaceSupported(Steinberg::Vst::IComponentHandler::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IComponentHandler2::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IComponentHandler3::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IHostApplication::iid);
    // Plugin-side interfaces the host interoperates with (queries / casts).
    addPlugInterfaceSupported(Steinberg::Vst::IEditController::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IEditController2::iid);
    // Messaging infrastructure (host creates IMessage via IHostApplication::
    // createInstance; both sides use IAttributeList on the message).
    addPlugInterfaceSupported(Steinberg::Vst::IMessage::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IAttributeList::iid);
    // Component<->controller connection points (used for split-component
    // plugins via ConnectionProxy in PluginInstance::setup()).
    addPlugInterfaceSupported(Steinberg::Vst::IConnectionPoint::iid);
    // Unit handler interfaces: nst3 does not fully implement IUnitHandler,
    // but the SDK's default HostApplication advertises it for compat with
    // plugins that probe host capabilities before deciding to use units.
    addPlugInterfaceSupported(Steinberg::Vst::IUnitHandler::iid);
    addPlugInterfaceSupported(Steinberg::Vst::IUnitHandler2::iid);
    // The host advertises IPlugInterfaceSupport itself so plugins can
    // discover that capability-probing is available.
    addPlugInterfaceSupported(Steinberg::Vst::IPlugInterfaceSupport::iid);
    // IParameterFinder: queried by some plugins to resolve a parameter from
    // a screen coordinate. Not fully exercised but advertised for compat.
    addPlugInterfaceSupported(Steinberg::Vst::IParameterFinder::iid);

    // Deliberately NOT advertised (GUI-only, out of scope for nst3):
    //   - IPlugFrame
    //   - IPlugView
    //   - IPlugViewContentScaleSupport
    //   - IContextMenu
    // Advertising these would cause plugins to attempt GUI embedding calls
    // the host cannot fulfill.
}

NstPlugInterfaceSupport::~NstPlugInterfaceSupport() noexcept = default;

//------------------------------------------------------------------------
// NstHostApplication
//------------------------------------------------------------------------
NstHostApplication::NstHostApplication() {
    // Install our curated IPlugInterfaceSupport, replacing the SDK default.
    // The base HostApplication constructor already created a default
    // PlugInterfaceSupport with the SDK's broad list; setPlugInterfaceSupport
    // releases that and acquires ours. We hold a separate reference via
    // nstPlugInterfaceSupport_ so the object's lifetime is tied to this
    // NstHostApplication regardless of the base class's internal IPtr.
    nstPlugInterfaceSupport_ = Steinberg::owned(new NstPlugInterfaceSupport());
    setPlugInterfaceSupport(nstPlugInterfaceSupport_.get());
}

NstHostApplication::~NstHostApplication() noexcept {
    // HostApplication base class dtor is responsible for releasing the
    // PlugInterfaceSupport held in its internal IPtr. We just clear our
    // raw pointer (the handler is owned elsewhere) and release our member
    // reference (the base's IPtr still holds one until the base dtor runs).
    handler_ = nullptr;
    nstPlugInterfaceSupport_.reset();
}

Steinberg::tresult PLUGIN_API NstHostApplication::getName(Steinberg::Vst::String128 name) {
    if (!name) return Steinberg::kInvalidArgument;
    static const std::string kHostName = "Node.js VST3 Host";
    utf8ToString128(kHostName, name);
    return Steinberg::kResultTrue;
}

} // namespace nst3
