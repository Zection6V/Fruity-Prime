#pragma once

// The process's QGuiApplication. It is made on first use rather than in main()
// so the headless paths (server, master server, tools) never load a Qt
// platform plugin or need a display.

namespace MphRead::Qt
{
    void EnsureApplication();
}
