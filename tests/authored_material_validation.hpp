#pragma once
#include "apple/metal_renderer.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <iostream>

/// Validates a real authored HLSL material on the hardware GPU with explicit reflected constants and texture bindings.
class AuthoredMaterialValidation final {
    /// Reads a cooked library fixture produced by the actual managed Apple shader contributor.
    static std::vector<std::uint8_t> Read(const std::string& path) {
        std::ifstream stream(path,std::ios::binary|std::ios::ate);
        if (!stream || stream.tellg() <= 0) throw std::runtime_error("Missing authored Metal library fixture: "+path);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stream.tellg()));
        stream.seekg(0);
        if (!stream.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Incomplete authored Metal library fixture: "+path);
        return bytes;
    }
public:
    /// Renders an indexed material, checks exact tinted pixels, and proves missing resources are rejected before drawing.
    static void Run(const std::string& root) {
        using namespace helengine::apple;
        MetalRenderer renderer;
        const auto vertex = Read(root+"/vertex.metallib");
        const auto fragment = Read(root+"/fragment.metallib");
        const auto material = renderer.UploadMaterial(vertex,"VSMain",fragment,"PSMain");
        const std::array<MeshVertex,4> vertices = {{{{-1,-1,0.5f},{0,0,1},{0,1}},{{1,-1,0.5f},{0,0,1},{1,1}},{{1,1,0.5f},{0,0,1},{1,0}},{{-1,1,0.5f},{0,0,1},{0,0}}}};
        const std::array<std::uint32_t,6> indices = {0,1,2,0,2,3};
        const auto mesh = renderer.UploadMesh(vertices,indices);
        const std::array<std::uint8_t,4> texel = {32,128,224,255};
        const auto texture = renderer.UploadTexture(1,1,texel);
        const std::array<float,16> matrix = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const std::array<float,4> color = {0.5f,1,0.25f,1};
        std::array<MaterialBuffer,2> buffers = {MaterialBuffer{0,1,std::vector<std::uint8_t>(64)},MaterialBuffer{1,2,std::vector<std::uint8_t>(16)}};
        std::memcpy(buffers[0].Bytes.data(),matrix.data(),64);
        std::memcpy(buffers[1].Bytes.data(),color.data(),16);
        bool rejected = false;
        try { renderer.SubmitMaterialMesh(mesh,material,buffers,{}); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Authored material accepted a missing required texture");
        const std::array<MaterialTexture,1> bindings = {MaterialTexture{0,2,texture}};
        renderer.SubmitMaterialMesh(mesh,material,buffers,bindings);
        const auto pixels = renderer.RenderOffscreen(64,64);
        const std::size_t center = (32*64+32)*4;
        if (pixels[center] != 16 || pixels[center+1] != 128 || pixels[center+2] != 56 || pixels[center+3] != 255) throw std::runtime_error("Actual authored Metal material sampled/tinted pixels incorrectly");
        renderer.ReleaseMaterial(material);
        std::cout << "Authored single-texture material pixels verified\n" << std::flush;
        const auto arrayFragment = Read(root+"/fragment-array.metallib");
        const auto arrayMaterial = renderer.UploadMaterial(vertex,"VSMain",arrayFragment,"PSMain");
        rejected = false;
        try { renderer.SubmitMaterialMesh(mesh,arrayMaterial,buffers,bindings); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Authored material accepted a missing second array texture");
        const std::array<MaterialTexture,2> arrayBindings = {MaterialTexture{0,2,texture},MaterialTexture{1,2,texture}};
        renderer.SubmitMaterialMesh(mesh,arrayMaterial,buffers,arrayBindings);
        const auto arrayPixels = renderer.RenderOffscreen(64,64);
        if (arrayPixels[center] != 16 || arrayPixels[center+1] != 128 || arrayPixels[center+2] != 56 || arrayPixels[center+3] != 255) throw std::runtime_error("Authored Metal texture/sampler array pixels were incorrect");
        renderer.ReleaseMaterial(arrayMaterial);
        std::cout << "Authored texture/sampler array pixels verified\n" << std::flush;
        renderer.ReleaseMesh(mesh);
        renderer.ReleaseTexture(texture);
    }
};
