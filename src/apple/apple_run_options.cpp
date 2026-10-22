#include "apple_run_options.hpp"
#include <charconv>
#include <filesystem>
#include <stdexcept>

namespace helengine::apple {
AppleRunOptions::AppleRunOptions(int argc,char** argv) {
    for (int index=1;index<argc;++index) {
        const std::string argument=argv[index];
        if (argument=="--offscreen") { Offscreen=true; }
        else if (argument=="--validation") { Validation=true; }
        else if (argument=="--content" || argument=="--scene" || argument=="--frames") {
            if (++index>=argc) { throw std::invalid_argument("Missing value for Apple player flag: " + argument); }
            const std::string value=argv[index];
            if (argument=="--content") { ContentRoot=std::filesystem::absolute(value).string(); }
            else if (argument=="--scene") { SceneId=value; }
            else { auto result=std::from_chars(value.data(),value.data()+value.size(),Frames); if (result.ec!=std::errc() || result.ptr!=value.data()+value.size() || Frames==0) { throw std::invalid_argument("--frames must be a positive integer."); } }
        } else { throw std::invalid_argument("Unknown Apple player flag: " + argument); }
    }
    if (ContentRoot.empty() && argc>0 && argv[0]) {
        const auto resources=std::filesystem::absolute(argv[0]).parent_path().parent_path()/"Resources";
        if (std::filesystem::is_directory(resources/"cooked")) ContentRoot=resources.string();
    }
    if (ContentRoot.empty() || !std::filesystem::is_directory(ContentRoot)) { throw std::invalid_argument("Apple player requires bundled cooked resources or --content with an existing cooked package directory."); }
    if (Offscreen && !Frames) { throw std::invalid_argument("Offscreen core validation requires an explicit positive --frames limit."); }
}
}
