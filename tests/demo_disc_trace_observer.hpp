#pragma once
#include "apple/core_player_observer.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "Component.hpp"
#include "SoftwarePathTracerComponent.hpp"
#include "SoftwarePathTraceSessionState.hpp"
#include <iostream>
#include <stdexcept>

namespace helengine::apple::tests {
/// Requires actual generated progressive CPU tracing, preventing an error HUD from passing a framebuffer smoke test.
class DemoDiscTraceObserver final : public CorePlayerObserver {
    /// Borrows the authored trace component while the loaded scene remains alive.
    ::SoftwarePathTracerComponent* Tracer = nullptr;
public:
    /// Keeps authored simulation and input untouched for every progressive tile update.
    void BeforeUpdate(::Core&,SdlInputBackend&,uint32_t) override { }
    /// Rejects caught component initialization/trace failures immediately after the real update path.
    void AfterUpdate(::Core& core,SdlInputBackend&,uint32_t) override {
        if (!Tracer) {
            auto* entities=core.get_ObjectManager()->get_Entities();
            for (int32_t index=0;index<entities->get_Count();++index) {
                auto* components=(*entities)[index]->get_Components();
                for (int32_t component=0;component<components->get_Count();++component) {
                    auto* trace=dynamic_cast<::SoftwarePathTracerComponent*>((*components)[component]);
                    if (trace) { if (Tracer) throw std::runtime_error("Trace validation found multiple progressive controllers"); Tracer=trace; }
                }
            }
        }
        if (!Tracer || Tracer->get_SessionState()!=::SoftwarePathTraceSessionState::Tracing) throw std::runtime_error("Actual generated software trace component failed initialization or left Tracing state");
    }
    /// Requires one complete authored 320x240 progressive pass and finite measured ray work.
    void ValidateComplete(::Core&) override {
        if (!Tracer) throw std::runtime_error("Trace validation has no authored controller");
        const auto spp=Tracer->get_CompletedSpp(); const auto rays=Tracer->get_TotalRays(); const auto nonfinite=Tracer->get_NonFiniteSampleCount();
        std::cout << "trace_spp=" << spp << " trace_rays=" << rays << " trace_nonfinite=" << nonfinite << '\n';
        if (spp<1 || rays<320*240 || nonfinite!=0) throw std::runtime_error("Generated CPU trace did not complete one finite full-resolution progressive pass");
    }
};
}
