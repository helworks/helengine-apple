#include "apple/apple_player.hpp"
#include "demo_disc_trace_observer.hpp"
#include <iostream>

/// Runs the actual generic player with project-specific progressive tracing assertions kept exclusively in test code.
int main(int argc,char** argv) {
    try {
        helengine::apple::AppleRunOptions options(argc,argv);
        if (!options.Offscreen || options.Frames<1200 || options.SceneId.empty()) throw std::invalid_argument("Trace validation requires explicit scene, offscreen mode and at least 1200 frames");
        helengine::apple::tests::DemoDiscTraceObserver observer;
        return helengine::apple::ApplePlayer::Run(options,&observer);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    catch (const std::exception* error) { std::cerr << (error ? error->what() : "null generated exception") << '\n'; delete error; }
    return 1;
}
