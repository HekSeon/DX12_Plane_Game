#pragma once
#include "RendererFactory.h"
#include "window/window.h"
#include <memory>
#include "game/game.h"
#include "sound/sound.h"

class Engine
{
  public:
    ~Engine();
    int Run();

  private:
    bool Initialize();
    void Update(float dt);
    void Render();

    Window m_window;
    Game m_game;
    Sound m_sound;
    std::unique_ptr<IRenderer> m_renderer;
};
