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
        auto translated=matrix; translated[0]=translated[5]=0.25f; translated[3]=0.5f;
        std::memcpy(buffers[0].Bytes.data(),translated.data(),64);
        renderer.SubmitMaterialMesh(mesh,material,buffers,bindings);
        const auto moved=renderer.RenderOffscreen(64,64);
        if (moved[(32*64+48)*4]!=16 || moved[(32*64+16)*4]!=0) throw std::runtime_error("Authored default-column-major row-vector transform packing is incorrect");
        std::memcpy(buffers[0].Bytes.data(),matrix.data(),64);
        renderer.ReleaseMaterial(material);
        std::cout << "Authored single-texture material pixels verified\n" << std::flush;
        const auto cubeFragment=Read(root+"/fragment-cube.metallib");
        const auto cubeMaterial=renderer.UploadMaterial(vertex,"VSMain",cubeFragment,"PSMain");
        rejected=false;
        try { renderer.SubmitMaterialMesh(mesh,cubeMaterial,buffers,bindings); } catch (const std::invalid_argument&) { rejected=true; }
        if (!rejected) throw std::runtime_error("Authored cube shader accepted a 2D texture");
        const std::array<uint8_t,24> faces={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255,32,128,224,255,255,0,255,255};
        const auto cubeTexture=renderer.UploadCubeTexture(1,faces);
        const std::array<MaterialTexture,1> cubeBindings={MaterialTexture{0,2,cubeTexture}};
        renderer.SubmitMaterialMesh(mesh,cubeMaterial,buffers,cubeBindings);
        const auto cubePixels=renderer.RenderOffscreen(64,64);
        if (cubePixels[center]!=16 || cubePixels[center+1]!=128 || cubePixels[center+2]!=56 || cubePixels[center+3]!=255) throw std::runtime_error("Actual authored cube sampling selected unexpected RGBA="+std::to_string(cubePixels[center])+","+std::to_string(cubePixels[center+1])+","+std::to_string(cubePixels[center+2])+","+std::to_string(cubePixels[center+3]));
        renderer.ReleaseMaterial(cubeMaterial); renderer.ReleaseTexture(cubeTexture);
        std::cout << "Authored cube +Z face pixels and dimension rejection verified\n" << std::flush;
        renderer.ReleaseMesh(mesh);
        renderer.ReleaseTexture(texture);
    }
};
