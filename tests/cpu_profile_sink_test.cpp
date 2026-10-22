#include "apple/apple_cpu_profile_sink.hpp"
#include "apple/apple_tracy_profiler.hpp"
#include "IUpdateable.hpp"
#include <iostream>
#include <stdexcept>

/// Implements the real generated updateable interface without constructing a scene or GPU.
class ProfileTestUpdateable final : public ::IUpdateable {
    /// Stores the generated interface's mutable update order.
    uint8_t Order=0;
public:
    /// Returns the update order without invoking Core work.
    uint8_t get_UpdateOrder() override { return Order; }
    /// Preserves the generated mutable update-order contract.
    void set_UpdateOrder(uint8_t value) override { Order=value; }
    /// Keeps explicit measured test samples independent from host scheduler timing.
    void Update() override {}
};

/// Verifies validated timings, zero-duration calls, stage bounds, frame reset and address re-registration.
int main() {
    using helengine::apple::AppleCpuProfileSink;
    // Tracy asynchronously retains plot pointers; production and this instrumented test keep the sink until process exit.
    auto& sink=*new AppleCpuProfileSink;
    ProfileTestUpdateable item;
    bool rejected=false;
    try { sink.RegisterType(nullptr,7,"invalid"); } catch(const std::invalid_argument&) { rejected=true; }
    if(!rejected) throw std::runtime_error("Null updateable registration was accepted.");
    for(auto stage:{static_cast<::RuntimeCpuProfileStage>(0),static_cast<::RuntimeCpuProfileStage>(255)}) {
        rejected=false; try { sink.RecordStage(stage,1); } catch(const std::out_of_range&) { rejected=true; }
        if(!rejected) throw std::runtime_error("Unknown Core stage was accepted.");
    }
    rejected=false; try { sink.RecordUpdateable(&item,0,7,12,1); } catch(const std::logic_error&) { rejected=true; }
    if(!rejected) throw std::runtime_error("An unregistered updateable sample was accepted.");
    sink.RegisterType(&item,7,"ProfileTestUpdateable");
    sink.RecordStage(::RuntimeCpuProfileStage::ObjectManagerUpdate,42);
    sink.RecordStage(::RuntimeCpuProfileStage::CommandListTrailingClips,8);
    sink.RecordUpdateable(&item,0,7,12,11); sink.RecordUpdateable(&item,0,7,12,19); sink.RecordUpdateable(&item,0,7,12,0); sink.ClockFault(); sink.FlushFrame();
    auto result=sink.GetLastFrameSummary();
    if(result.ExecutedComponents!=1||result.Calls!=3||result.ComponentMicroseconds!=30||result.StageMicroseconds!=50||result.ClockFaults!=1) throw std::runtime_error("Apple CPU frame totals differ from actual submitted samples.");
    sink.RegisterType(&item,8,"ReplacementAtSameAddress");
    rejected=false; try { sink.RecordUpdateable(&item,0,7,13,100); } catch(const std::logic_error&) { rejected=true; }
    if(!rejected) throw std::runtime_error("An obsolete registration hash was accepted.");
    sink.RecordUpdateable(&item,0,8,13,3); sink.FlushFrame(); result=sink.GetLastFrameSummary();
    if(result.ExecutedComponents!=1||result.Calls!=1||result.ComponentMicroseconds!=3||result.StageMicroseconds!=0||result.ClockFaults!=1) throw std::runtime_error("Apple CPU frame reset or address re-registration failed.");
    sink.FlushFrame(); result=sink.GetLastFrameSummary();
    if(result.ExecutedComponents||result.Calls||result.ComponentMicroseconds||result.StageMicroseconds) throw std::runtime_error("Idle CPU frame retained old timing samples.");
    const auto before=sink.ReadMicroseconds(),after=sink.ReadMicroseconds();
    if(!before||after<before) throw std::runtime_error("Apple CPU profiler clock is not monotonic.");
    auto* singleton=helengine::apple::GetAppleCpuProfileSink();
    if(!singleton||singleton!=helengine::apple::GetAppleCpuProfileSink()) throw std::runtime_error("Instrumented Apple sink identity is unstable.");
    rejected=false; try { helengine::apple::FlushAppleProfilerFrame(nullptr); } catch(const std::invalid_argument&) { rejected=true; }
    if(!rejected) throw std::runtime_error("Profiler frame accepted a missing Core.");
    std::cout<<"apple_cpu_profile_sink=passed\n";
}
