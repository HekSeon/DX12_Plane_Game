#include "engine/Engine.h"
#include <exception>
#include <iostream>

int main()
{
    try
    {
        Engine engine;
        return engine.Run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "DX12 Plane Game: " << error.what() << '\n';
        return 1;
    }
}
