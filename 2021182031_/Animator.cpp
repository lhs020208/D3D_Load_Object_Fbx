// Animator.cpp
#define NOMINMAX                // Windows min/max 매크로 비활성화 (std::min 충돌 방지)
#include "stdafx.h"
#include "Animator.h"
#include "Mesh.h"               // ← Bone의 '정의'가 필요하므로 반드시 포함
#include <algorithm>
#include <cmath>
using namespace DirectX;

void CAnimator::Update(double dt) {
    m_CurrentTime += dt;
}

void CAnimator::Update(double dt, const std::vector<Bone>& bones, XMFLOAT4X4* outBoneTransforms)
{
    m_CurrentTime += dt;
    if (!outBoneTransforms) return;

    for (size_t i = 0; i < bones.size(); ++i)
    {
        const auto& keys = bones[i].keyframes;
        if (keys.empty()) {
            XMStoreFloat4x4(&outBoneTransforms[i], XMMatrixIdentity());
            continue;
        }

        double end = keys.back().time;
        double t = (end > 0.0) ? fmod(m_CurrentTime, end) : m_CurrentTime;

        int k1 = 0;
        while (k1 + 1 < (int)keys.size() && keys[k1 + 1].time <= t) ++k1;
        int k2 = (k1 + 1 < (int)keys.size()) ? k1 + 1 : (int)keys.size() - 1;

        if (k1 == k2) {
            XMStoreFloat4x4(&outBoneTransforms[i], XMLoadFloat4x4(&keys[k1].transform));
            continue;
        }

        double t1 = keys[k1].time, t2 = keys[k2].time;
        const double denom = (std::max)(1e-8, t2 - t1);   // ← 소괄호 트릭
        float a = static_cast<float>((t - t1) / denom);

        // TRS 분해 후 보간 (XMMatrixLerp 대신 안전)
        XMVECTOR S1, R1, T1v, S2, R2, T2v;
        XMMATRIX M1 = XMLoadFloat4x4(&keys[k1].transform);
        XMMATRIX M2 = XMLoadFloat4x4(&keys[k2].transform);
        XMMatrixDecompose(&S1, &R1, &T1v, M1);
        XMMatrixDecompose(&S2, &R2, &T2v, M2);

        XMVECTOR S = XMVectorLerp(S1, S2, a);
        XMVECTOR R = XMQuaternionSlerp(R1, R2, a);
        XMVECTOR T = XMVectorLerp(T1v, T2v, a);

        XMMATRIX M = XMMatrixAffineTransformation(S, XMVectorZero(), R, T);
        XMStoreFloat4x4(&outBoneTransforms[i], M);
    }
}
