//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// HostApplication implementation
//-----------------------------------------------------------------------------
#include "host_application.h"

#include "string_convert.h"

// SDK interface headers — needed for the IXXX::iid FUID constants that we
// advertise via NstPlugInterfaceSupport.
#include "pluginterfaces/vst/ivsteditcontroller.h"       // IComponentHandler{,2}, IEditController{,2}
#include "pluginterfaces/vst/ivstcontextmenu.h"          // IComponentHandler3
#include "pluginterfaces/vst/ivstplugview.h"             // IParameterFinder
#include "pluginterfaces/vst/ivsthostapplication.h"      // IHostApplication
#include "pluginterfaces/vst/ivstmessage.h"              // IMessage, IAttributeList, IConnectionPoint
#include "pluginterfaces/vst/ivstunits.h"                // IUnitHandler{,2}
#include "pluginterfaces/vst/ivstpluginterfacesupport.h" // IPlugInterfaceSupport
#include "pluginterfaces/base/funknown.h"

// Reuse the SDK's example HostMessage + HostAttributeList implementations
// for IHostApplication::createInstance("IMessage"/"IAttributeList"). These
// are exactly what the SDK HostApplication base class returned previously.
#include "public.sdk/source/vst/hosting/hostclasses.h"

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
    // Construct our curated PlugInterfaceSupport up front. This replaces the
    // previous approach of subclassing Steinberg::Vst::HostApplication and
    // trying to reassign its (private) mPlugInterfaceSupport member, which
    // never compiled.
    nstPlugInterfaceSupport_ = Steinberg::owned(new NstPlugInterfaceSupport());
}

NstHostApplication::~NstHostApplication() noexcept {
    // Drop our strong reference. Any plugin still holding a reference to the
    // IPlugInterfaceSupport (obtained via queryInterface) keeps it alive
    // through its own IPtr.
    handler_ = nullptr;
    nstPlugInterfaceSupport_.reset();
}

Steinberg::tresult PLUGIN_API NstHostApplication::getName(Steinberg::Vst::String128 name) {
    if (!name) return Steinberg::kInvalidArgument;
    static const std::string kHostName = "Node.js VST3 Host";
    utf8ToString128(kHostName, name);
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API NstHostApplication::createInstance(Steinberg::TUID cid,
                                                                Steinberg::TUID _iid,
                                                                void** obj) {
    if (!obj) return Steinberg::kInvalidArgument;
    *obj = nullptr;

    // Reuse the SDK's example HostMessage / HostAttributeList so plugins get
    // the same IMessage / IAttributeList implementation the SDK HostApplication
    // would have returned. We construct them directly here (instead of
    // delegating to the base class) because we no longer inherit from
    // HostApplication.
    if (Steinberg::FUnknownPrivate::iidEqual(cid, Steinberg::Vst::IMessage::iid) &&
        Steinberg::FUnknownPrivate::iidEqual(_iid, Steinberg::Vst::IMessage::iid)) {
        auto* msg = new Steinberg::Vst::HostMessage;
        msg->addRef(); // queryInterface contract: returned with refcount bump
        *obj = msg;
        return Steinberg::kResultTrue;
    }
    if (Steinberg::FUnknownPrivate::iidEqual(cid, Steinberg::Vst::IAttributeList::iid) &&
        Steinberg::FUnknownPrivate::iidEqual(_iid, Steinberg::Vst::IAttributeList::iid)) {
        if (auto al = Steinberg::Vst::HostAttributeList::make()) {
            *obj = al.take();
            return Steinberg::kResultTrue;
        }
        return Steinberg::kOutOfMemory;
    }
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API NstHostApplication::queryInterface(const Steinberg::TUID _iid,
                                                                 void** obj) {
    if (!obj) return Steinberg::kInvalidArgument;
    *obj = nullptr;

    // Standard FUnknown interfaces.
    QUERY_INTERFACE(_iid, obj, Steinberg::FUnknown::iid, Steinberg::FUnknown)
    QUERY_INTERFACE(_iid, obj, Steinberg::Vst::IHostApplication::iid,
                    Steinberg::Vst::IHostApplication)

    // Forward to our curated PlugInterfaceSupport so plugins querying for
    // IPlugInterfaceSupport obtain it directly from the host context —
    // this is exactly what the SDK HostApplication base class did.
    if (nstPlugInterfaceSupport_ &&
        nstPlugInterfaceSupport_->queryInterface(_iid, obj) == Steinberg::kResultTrue) {
        return Steinberg::kResultOk;
    }

    *obj = nullptr;
    return Steinberg::kNoInterface;
}

Steinberg::uint32 PLUGIN_API NstHostApplication::addRef() {
    // Singleton-style: lifetime is owned by the Host JS wrapper, not by COM.
    // Matches Steinberg::Vst::HostApplication::addRef() behavior.
    return 1;
}

Steinberg::uint32 PLUGIN_API NstHostApplication::release() {
    // Singleton-style: see addRef() comment. Never let refcount drop to 0
    // through COM release, since the Host JS wrapper owns the C++ instance.
    return 1;
}

} // namespace nst3
