#include "engine/Engine.h"
#include <exception>
#include <Windows.h>

// Windows giriş noktası konsol oluşturmadan oyunu başlatır.
// The Windows entry point starts the game without creating a console.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        Engine engine;
        int result = engine.Run();
        if (result != 0)
            MessageBoxW(nullptr, L"Oyun baslatilamadi.", L"DX12 Plane Game", MB_OK | MB_ICONERROR);
        return result;
    }
    catch (const std::exception &error)
    {
        MessageBoxA(nullptr, error.what(), "DX12 Plane Game", MB_OK | MB_ICONERROR);
        return 1;
    }
}
