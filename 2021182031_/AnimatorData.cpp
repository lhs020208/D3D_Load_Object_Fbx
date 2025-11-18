#include "stdafx.h"
#include "AnimatorData.h"

using namespace DirectX;

// TRS → Matrix 조합 헬퍼
static inline void ComposeTRS(
    const XMFLOAT3& T,
    const XMFLOAT4& Q,
    const XMFLOAT3& S,
    XMFLOAT4X4& outM)
{
    XMVECTOR t = XMLoadFloat3(&T);
    XMVECTOR q = XMLoadFloat4(&Q);
    XMVECTOR s = XMLoadFloat3(&S);

    q = XMQuaternionNormalize(q);

    XMMATRIX M =
        XMMatrixScalingFromVector(s) *
        XMMatrixRotationQuaternion(q) *
        XMMatrixTranslationFromVector(t);

    XMStoreFloat4x4(&outM, M);
}

// timeSec 시점의 로컬 본 행렬들 계산
void AnimationClip::Evaluate(float timeSec, std::vector<XMFLOAT4X4>& outLocalTransforms) const
{
    const size_t boneCount = boneTracks.size();
    if (outLocalTransforms.size() < boneCount)
        outLocalTransforms.resize(boneCount);

    // 시간 클램프 (0 ~ duration)
    if (duration > 0.0f)
    {
        if (timeSec < 0.0f)       timeSec = 0.0f;
        else if (timeSec > duration) timeSec = duration;
    }

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    for (size_t i = 0; i < boneCount; ++i)
    {
        const BoneKeyframes& track = boneTracks[i];
        const auto& keys = track.keyframes;

        // 키가 없으면 identity
        if (keys.empty())
        {
            outLocalTransforms[i] = identity;
            continue;
        }

        // 키가 하나 뿐이거나, 첫 키 이전 → 첫 키 사용
        if (keys.size() == 1 || timeSec <= keys.front().timeSec)
        {
            ComposeTRS(keys.front().translation,
                keys.front().rotationQuat,
                keys.front().scale,
                outLocalTransforms[i]);
            continue;
        }

        // 마지막 키 이후 → 마지막 키 사용
        if (timeSec >= keys.back().timeSec)
        {
            ComposeTRS(keys.back().translation,
                keys.back().rotationQuat,
                keys.back().scale,
                outLocalTransforms[i]);
            continue;
        }

        // 사이에 있는 구간 찾기 (간단히 선형 탐색)
        size_t k1 = 1;
        while (k1 < keys.size() && keys[k1].timeSec < timeSec)
            ++k1;
        size_t k0 = k1 - 1;

        const Keyframe& kf0 = keys[k0];
        const Keyframe& kf1 = keys[k1];

        float t0 = kf0.timeSec;
        float t1 = kf1.timeSec;
        float denom = (t1 - t0);
        float alpha = (denom > 0.0f) ? (timeSec - t0) / denom : 0.0f;

        // 위치/스케일: 선형 보간
        XMVECTOR T0 = XMLoadFloat3(&kf0.translation);
        XMVECTOR T1 = XMLoadFloat3(&kf1.translation);
        XMVECTOR S0 = XMLoadFloat3(&kf0.scale);
        XMVECTOR S1 = XMLoadFloat3(&kf1.scale);

        XMVECTOR T = XMVectorLerp(T0, T1, alpha);
        XMVECTOR S = XMVectorLerp(S0, S1, alpha);

        // 회전: 쿼터니언 SLERP
        XMVECTOR R0 = XMLoadFloat4(&kf0.rotationQuat);
        XMVECTOR R1 = XMLoadFloat4(&kf1.rotationQuat);
        R0 = XMQuaternionNormalize(R0);
        R1 = XMQuaternionNormalize(R1);

        XMVECTOR R = XMQuaternionSlerp(R0, R1, alpha);

        XMFLOAT3 outT, outS;
        XMFLOAT4 outR;
        XMStoreFloat3(&outT, T);
        XMStoreFloat3(&outS, S);
        XMStoreFloat4(&outR, R);

        ComposeTRS(outT, outR, outS, outLocalTransforms[i]);
    }
}
