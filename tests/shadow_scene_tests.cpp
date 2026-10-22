#include "apple/apple_player.hpp"
#include "shadow_scene_observer.hpp"
#include <iostream>

/// Validates packaged authored shadow scenes through the same Core and native Metal renderer as the actual player.
int main(int argc,char** argv) {
    try {
        helengine::apple::AppleRunOptions options(argc,argv);
        if (!options.Offscreen || options.Frames!=3 || options.SceneId.empty()) throw std::invalid_argument("Shadow receiver test requires an explicit scene and exactly three offscreen update frames");
        helengine::apple::tests::ShadowSceneObserver observer;
        return helengine::apple::ApplePlayer::Run(options,&observer);
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    catch (const std::exception* error) { std::cerr << (error ? error->what() : "null generated exception") << '\n'; delete error; }
    return 1;
}
