#include "apple/apple_player.hpp"
#include "demo_disc_scene_return_observer.hpp"
#include <iostream>

/// Runs one actual packaged rendering scene and returns using the selected native input device.
int main(int argc,char** argv) {
    try {
        if (argc<2 || (std::string(argv[1])!="keyboard" && std::string(argv[1])!="gamepad")) throw std::invalid_argument("Scene return test requires keyboard or gamepad followed by the normal player arguments");
        const bool gamepad=std::string(argv[1])=="gamepad";
        helengine::apple::AppleRunOptions options(argc-1,argv+1);
        if (!options.Offscreen || options.Frames!=120 || options.SceneId.empty() || options.SceneId=="DemoDiscMainMenu") throw std::invalid_argument("Scene return test requires one rendering scene and exactly 120 offscreen frames");
        helengine::apple::tests::DemoDiscSceneReturnObserver observer(gamepad);
        return helengine::apple::ApplePlayer::Run(options,&observer);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    catch (const std::exception* error) { std::cerr << (error ? error->what() : "null generated exception") << '\n'; delete error; }
    return 1;
}
