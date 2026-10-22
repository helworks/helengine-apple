#pragma once
class Core;
class IRuntimeCpuProfileSink;

namespace helengine::apple {
/// <summary>Returns a process-lifetime sink only for a Tracy-instrumented native player.</summary>
::IRuntimeCpuProfileSink* GetAppleCpuProfileSink();
/// <summary>Publishes component/entity/stage counters after the measured Core frame and emits its Tracy frame marker.</summary>
void FlushAppleProfilerFrame(::Core* core);
}


