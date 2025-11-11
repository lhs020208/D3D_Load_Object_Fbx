#pragma once
#include "stdafx.h"

struct Keyframe { double time; XMFLOAT4X4 transform; };
struct BoneAnimation { std::vector<Keyframe> keys; };

class CAnimator {
public:
    std::vector<BoneAnimation> m_BoneAnims;
    double m_CurrentTime = 0.0;

    void Update(double dt) {
        m_CurrentTime += dt;
    }
};