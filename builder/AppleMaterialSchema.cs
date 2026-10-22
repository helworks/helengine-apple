using helengine.baseplatform.Definitions;

namespace helengine.apple.builder;

/// <summary>Declares the common engine standard shader fields without coupling the Apple backend to DirectX.</summary>
public static class AppleMaterialSchema {
    /// <summary>Creates the authored fields serialized by AppleMaterialCooker.</summary>
    public static PlatformMaterialSchemaDefinition Create() {
        return new("standard-shader", "Standard Shader", ["metal"], [
            new("use-custom-shader", "Use Custom Shader", PlatformMaterialFieldKind.Boolean, "false", true, []),
            new("shader-asset-id", "Shader Asset", PlatformMaterialFieldKind.AssetReference, "", true, []),
            new("vertex-program", "Vertex Program", PlatformMaterialFieldKind.Text, "", true, []),
            new("pixel-program", "Pixel Program", PlatformMaterialFieldKind.Text, "", true, []),
            new("variant", "Variant", PlatformMaterialFieldKind.Text, "default", true, []),
            new("base-color", "Base Color", PlatformMaterialFieldKind.Color, "#ffffff", false, []),
            new("texture-id", "Diffuse Texture", PlatformMaterialFieldKind.AssetReference, "", false, []),
            new("roughness", "Roughness", PlatformMaterialFieldKind.Text, "0.5", false, []),
            new("roughness-texture-id", "Roughness Texture", PlatformMaterialFieldKind.AssetReference, "", false, []),
            new("metallic", "Metallic", PlatformMaterialFieldKind.Text, "0", false, []),
            new("specular", "Specular", PlatformMaterialFieldKind.Text, "0.5", false, []),
            new("casts-shadow", "Casts Shadows", PlatformMaterialFieldKind.Boolean, "true", false, []),
            new("receives-shadow", "Receives Shadows", PlatformMaterialFieldKind.Boolean, "true", false, [])
        ]);
    }
}
