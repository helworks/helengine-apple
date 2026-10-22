#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <iostream>

/// Loads an authored compiler-produced library into the real Metal device and checks its requested engine entry point.
int main(int argc,char** argv) {
    @autoreleasepool {
        if (argc != 3) { std::cerr << "Usage: metal_authored_library_tests <metallib> <entry-point>\n"; return 2; }
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) { std::cerr << "No Metal device available\n"; return 1; }
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]] error:&error];
        if (!library) { std::cerr << "Authored Metal library rejected: " << [[error localizedDescription] UTF8String] << '\n'; return 1; }
        id<MTLFunction> entry = [library newFunctionWithName:[NSString stringWithUTF8String:argv[2]]];
        if (!entry) { std::cerr << "Authored Metal library did not preserve entry point " << argv[2] << '\n'; return 1; }
        std::cout << "Authored Metal library loaded on " << [[device name] UTF8String] << ": " << argv[2] << '\n';
        return 0;
    }
}
