#include "apple/apple_player.hpp"
#include "demo_disc_menu_observer.hpp"
#include <iostream>

/// Exercises the same actual Core player while keeping all project-specific assertions outside production host code.
int main(int argc,char** argv) {
    try {
        helengine::apple::AppleRunOptions options(argc,argv);
        if (!options.Offscreen || options.Frames<300) throw std::invalid_argument("DemoDisc menu test requires --offscreen and at least 300 frames");
        helengine::apple::tests::DemoDiscMenuObserver observer;
        return helengine::apple::ApplePlayer::Run(options,&observer);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    catch (const std::exception* error) { std::cerr << (error ? error->what() : "null generated exception") << '\n'; delete error; }
    return 1;
}
