#if defined _DEBUG && defined _WIN32
    // CRT Memory Leak detection
    #define _CRTDBG_MAP_ALLOC  
    #include <stdlib.h>  
    #include <crtdbg.h>
#endif


#include <Core/SolEngine.h>
#include <spdlog/spdlog.h>

int main() {

#if defined _DEBUG && defined _WIN32
    // Detects memory leaks upon program exit
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const auto console = spdlog::stdout_color_mt("console");

    SolEngine engine;
    engine.init();

    engine.update();

    engine.shutdown();

    return 0;
}

