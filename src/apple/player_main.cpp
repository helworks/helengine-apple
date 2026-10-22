#include "apple/apple_player.hpp"
#include <exception>
#include <iostream>

/// Runs only the actual generated player and preserves both native and generated exception diagnostics.
int main(int argc,char** argv) {
    try { return helengine::apple::ApplePlayer::Run(helengine::apple::AppleRunOptions(argc,argv)); }
    catch (const std::exception& error) { std::cerr << "Apple core player: " << error.what() << '\n'; }
    catch (const std::exception* error) { std::cerr << "Apple generated core: " << (error ? error->what() : "null exception") << '\n'; delete error; }
    catch (...) { std::cerr << "Apple core player failed with an unknown exception\n"; }
    return 1;
}
