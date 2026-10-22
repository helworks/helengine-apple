#include <spirv_msl.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace helengine::apple {
    /// Remaps engine unified descriptor slots into Metal's separate native buffer, texture and sampler namespaces.
    class MetalShaderTranslator final {
        /// Reads an aligned SPIR-V module without changing its descriptor decorations or reflection identity.
        static std::vector<std::uint32_t> ReadWords(const std::string& path) {
            std::ifstream stream(path,std::ios::binary|std::ios::ate);
            if (!stream) throw std::runtime_error("Cannot open SPIR-V input: "+path);
            const auto bytes = stream.tellg();
            if (bytes <= 0 || bytes%4 != 0) throw std::runtime_error("SPIR-V input requires a nonempty aligned word stream");
            std::vector<std::uint32_t> words(static_cast<std::size_t>(bytes)/4);
            stream.seekg(0);
            if (!stream.read(reinterpret_cast<char*>(words.data()),bytes)) throw std::runtime_error("Failed to read complete SPIR-V input");
            return words;
        }

        /// Adds explicit per-kind native bindings while preserving the original SPIR-V set/binding pair for lookup.
        static void MapResources(spirv_cross::CompilerMSL& compiler,const spirv_cross::SmallVector<spirv_cross::Resource>& resources,spv::ExecutionModel stage,std::uint32_t shift,int kind) {
            for (const auto& resource : resources) {
                const auto slot = compiler.get_decoration(resource.id,spv::DecorationBinding);
                const auto set = compiler.get_decoration(resource.id,spv::DecorationDescriptorSet);
                if (set != 0) throw std::runtime_error("Metal explicit binding adapter currently requires descriptor space zero");
                if (slot < shift) throw std::runtime_error("SPIR-V binding is below its engine resource-kind base");
                const auto native = slot-shift;
                const auto& type = compiler.get_type(resource.type_id);
                std::uint32_t count = 1;
                for (std::size_t index=0; index<type.array.size(); ++index) {
                    if (!type.array_size_literal[index] || type.array[index] == 0 || count>std::numeric_limits<std::uint32_t>::max()/type.array[index]) throw std::runtime_error("Metal resource arrays require a finite reflected constant size");
                    count *= type.array[index];
                }
                if (count != 1) throw std::runtime_error("Authored Metal resource arrays are not supported by the validated native pipeline; explicit individual bindings are required");
                const auto limit = kind == 2 ? 16u : (kind == 1 ? 128u : 30u);
                if (native >= limit || count>limit-native) throw std::runtime_error("Authored shader resource exceeds Metal native per-kind binding limits");
                spirv_cross::MSLResourceBinding binding;
                binding.stage = stage;
                binding.desc_set = set;
                binding.binding = slot;
                binding.count = count;
                if (kind == 0) binding.msl_buffer = native;
                else if (kind == 1) binding.msl_texture = native;
                else binding.msl_sampler = native;
                compiler.add_msl_resource_binding(binding);
            }
        }

    public:
        /// Translates one selected shader entry point with explicit engine policy shifts; buffer slot 30 remains reserved for mesh vertices.
        static void Translate(char** arguments) {
            spirv_cross::CompilerMSL compiler(ReadWords(arguments[1]));
            const auto stage = std::string(arguments[3]) == "vert" ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
            if (std::string(arguments[3]) != "vert" && std::string(arguments[3]) != "frag") throw std::runtime_error("Metal translator stage must be vert or frag");
            compiler.set_entry_point(arguments[4],stage);
            auto options = compiler.get_msl_options();
            options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2,3);
            options.platform = std::string(arguments[5]) == "ios" ? spirv_cross::CompilerMSL::Options::iOS : spirv_cross::CompilerMSL::Options::macOS;
            compiler.set_msl_options(options);
            const auto resources = compiler.get_shader_resources();
            if (!resources.storage_buffers.empty() || !resources.storage_images.empty() || !resources.sampled_images.empty() || !resources.push_constant_buffers.empty() || !resources.subpass_inputs.empty()) throw std::runtime_error("Metal binding adapter requires explicit uniform buffers, separate textures and separate samplers");
            MapResources(compiler,resources.uniform_buffers,stage,static_cast<std::uint32_t>(std::stoul(arguments[6])),0);
            MapResources(compiler,resources.separate_images,stage,static_cast<std::uint32_t>(std::stoul(arguments[7])),1);
            MapResources(compiler,resources.separate_samplers,stage,static_cast<std::uint32_t>(std::stoul(arguments[8])),2);
            const std::string source = compiler.compile();
            std::ofstream output(arguments[2],std::ios::binary|std::ios::trunc);
            if (!output || !output.write(source.data(),static_cast<std::streamsize>(source.size()))) throw std::runtime_error("Cannot write translated Metal source");
        }
    };
}

/// Reports shader translation failures with a native nonzero status instead of emitting partial MSL as success.
int main(int argc,char** argv) {
    if (argc != 9) { std::cerr << "Usage: helengine_msl_translate input.spv output.metal vert|frag entry macos|ios buffer-shift texture-shift sampler-shift\n"; return 2; }
    try { helengine::apple::MetalShaderTranslator::Translate(argv); return 0; }
    catch (const std::exception& error) { std::cerr << "Metal resource translation: " << error.what() << '\n'; return 1; }
}
