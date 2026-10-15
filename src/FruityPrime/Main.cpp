// The process entry point.
//
// C# has no file for this: the runtime calls Program.Main itself. C++ needs a
// main() to hand the arguments over, and that is all this file does.

#include "Program.hpp"
#include "Mods/MapGen/CustomRooms.hpp"
#include "NativeRuntime/System/Runtime.hpp"

#include <string>
#include <vector>

int main(int argc, char** argv)
{
    // args in C# excludes the program name.
    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i)
    {
        args.emplace_back(argv[i]);
    }
    MphRead::Program::Main(args);
    // The background map build reads statics that returning from main
    // destroys: it is stopped and joined first.
    MphRead::Mods::MapGen::CustomRooms::StopGenerating();
    return MphRead::NativeRuntime::EnvironmentExitCode();
}
