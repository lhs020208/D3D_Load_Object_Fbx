#include "stdafx.h"
#include "Sound.h"
#include <cassert>

FMOD::System* Sound::m_pSystem = nullptr;

void Sound::Init()
{
    FMOD_RESULT r;
    r = FMOD::System_Create(&m_pSystem);
    assert(r == FMOD_OK);

    r = m_pSystem->init(512, FMOD_INIT_NORMAL, nullptr);
    assert(r == FMOD_OK);
}

void Sound::Update()
{
    if (m_pSystem)
        m_pSystem->update();
}

void Sound::Release()
{
    if (m_pSystem)
    {
        m_pSystem->close();
        m_pSystem->release();
        m_pSystem = nullptr;
    }
}

void Sound::Play2D(const char* path)
{
    FMOD::Sound* sound = nullptr;
    m_pSystem->createSound(path, FMOD_DEFAULT, nullptr, &sound);
    m_pSystem->playSound(sound, nullptr, false, nullptr);
}


void Sound::PlayTest()
{
    FMOD::Sound* sound = nullptr;
    FMOD::Channel* channel = nullptr;
    FMOD_RESULT r;

    r = m_pSystem->createSound(
        "Sound/damage.wav",
        FMOD_DEFAULT,
        nullptr,
        &sound
    );
    assert(r == FMOD_OK);

    r = m_pSystem->playSound(sound, nullptr, false, &channel);
    assert(r == FMOD_OK);

    // º¼·ý Á¶Àý (0.0f ~ 1.0f)
    if (channel)
        channel->setVolume(0.3f);   // 30% º¼·ý
}

