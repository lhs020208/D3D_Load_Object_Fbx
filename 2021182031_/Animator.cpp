// Animator.cpp
#define NOMINMAX 
#include "stdafx.h"
#include "Animator.h"
#include "Mesh.h"
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

    // 1) 전체 클립 길이를 통일(모든 본의 마지막 키 시간의 최대값)
    double clipEnd = 0.0;
    for (const auto& b : bones)
        if (!b.keyframes.empty()) {
            double last = b.keyframes.back().time;
            if (last > clipEnd) clipEnd = last;
        }
    if (clipEnd <= 0.0) clipEnd = m_CurrentTime;
    double tWrap = fmod(m_CurrentTime, clipEnd);

    // 2) 본별 현재 '글로벌' 변환 보간
    for (size_t i = 0; i < bones.size(); ++i)
    {
        const auto& keys = bones[i].keyframes;
        if (keys.empty()) {
            XMStoreFloat4x4(&outBoneTransforms[i], XMMatrixIdentity());
            continue;
        }

        // 키 구간 탐색
        int k1 = 0;
        while (k1 + 1 < (int)keys.size() && keys[k1 + 1].time <= tWrap) ++k1;
        int k2 = (k1 + 1 < (int)keys.size()) ? k1 + 1 : k1;

        if (k1 == k2) {
            // 현재 글로벌 = 키프레임 글로벌
            // 최종 팔레트 = 현재글로벌 × 역-바인드
            XMMATRIX Mglobal = XMLoadFloat4x4(&keys[k1].transform);
            XMMATRIX MoffInv = XMLoadFloat4x4(&bones[i].offsetMatrix); // 여기에 '역-바인드'가 있어야 함
            XMStoreFloat4x4(&outBoneTransforms[i], XMMatrixMultiply(Mglobal, MoffInv));
            continue;
        }

        // 보간
        double t1 = keys[k1].time, t2 = keys[k2].time;
        double diff = (t2 - t1);
        if (diff < 1e-8) diff = 1e-8;
        float a = (float)((tWrap - t1) / diff);

        XMVECTOR S1, R1, T1v, S2, R2, T2v;
        XMMATRIX M1 = XMLoadFloat4x4(&keys[k1].transform);
        XMMATRIX M2 = XMLoadFloat4x4(&keys[k2].transform);
        XMMatrixDecompose(&S1, &R1, &T1v, M1);
        XMMatrixDecompose(&S2, &R2, &T2v, M2);

        XMVECTOR S = XMVectorLerp(S1, S2, a);
        XMVECTOR R = XMQuaternionSlerp(R1, R2, a);
        XMVECTOR T = XMVectorLerp(T1v, T2v, a);

        XMMATRIX Mglobal = XMMatrixAffineTransformation(S, XMVectorZero(), R, T);

        // ★ 최종 팔레트 = 현재글로벌 × 역-바인드
        XMMATRIX MoffInv = XMLoadFloat4x4(&bones[i].offsetMatrix);
        XMMATRIX Mskin = XMMatrixMultiply(Mglobal, MoffInv);
        XMStoreFloat4x4(&outBoneTransforms[i], Mskin);
    }
}

