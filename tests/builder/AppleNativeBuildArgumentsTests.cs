using System.Reflection;
using helengine.apple.builder;

/// <summary>Checks that Apple native builds preserve CMake's existing parallelism environment contract.</summary>
public static class AppleNativeBuildArgumentsTests {
    /// <summary>Retains six jobs when unset while allowing CMake to interpret defined numeric, empty and invalid values.</summary>
    public static void Run() {
        MethodInfo create = typeof(AppleBuildWorkspace).GetMethod("CreateNativeBuildArguments", BindingFlags.Static | BindingFlags.NonPublic)
            ?? throw new Exception("Apple native build argument construction is missing.");
        IReadOnlyList<string> defaults = (IReadOnlyList<string>)create.Invoke(null, ["native path", "Debug", null]);
        if (!defaults.SequenceEqual(["--build", "native path", "--config", "Debug", "-j", "6"])) { throw new Exception("Unset CMake parallelism changed the compatible six-job default."); }
        foreach (string configured in new[] { "2", "", "invalid" }) {
            IReadOnlyList<string> selected = (IReadOnlyList<string>)create.Invoke(null, ["native path", "Debug", configured]);
            if (!selected.SequenceEqual(["--build", "native path", "--config", "Debug"])) { throw new Exception("Explicit arguments override CMAKE_BUILD_PARALLEL_LEVEL instead of letting CMake interpret it."); }
        }
    }
}
