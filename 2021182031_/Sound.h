#pragma once
#include <fmod.hpp>

class Sound
{
public:
    static void Init();
    static void Update();
    static void Release();

    static void Play2D(const char* path);
    static void PlayTest();

private:
    static FMOD::System* m_pSystem;
};
