// AnimatorData.cpp
#include "stdafx.h"
#include "AnimatorData.h"

using namespace DirectX;

static void BuildTRSMatrix(
    const XMFLOAT3& t,
    const XMFLOAT4& r,
    const XMFLOAT3& s,
    XMFLOAT4X4& outM)
{
    XMVECTOR trans = XMLoadFloat3(&t);
    XMVECTOR rot = XMLoadFloat4(&r);
    XMVECTOR scale = XMLoadFloat3(&s);

    XMMATRIX mS = XMMatrixScalingFromVector(scale);
    XMMATRIX mR = XMMatrixRotationQuaternion(rot);
    XMMATRIX mT = XMMatrixTranslationFromVector(trans);

    // (Scale * Rotate * Translate) 순서
    XMMATRIX M = mS * mR * mT;
    XMStoreFloat4x4(&outM, M);
}

// 한 본의 키프레임 리스트에서 t에 해당하는 TRS를 보간해서 구함
static void SampleBoneTrack(
    const std::vector<Keyframe>& keys,
    float timeSec,
    XMFLOAT3& outT,
    XMFLOAT4& outR,
    XMFLOAT3& outS)
{
    const size_t keyCount = keys.size();
    if (keyCount == 0)
    {
        // 키가 없으면 단위 TRS
        outT = XMFLOAT3(0.f, 0.f, 0.f);
        outR = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
        outS = XMFLOAT3(1.f, 1.f, 1.f);
        return;
    }

    if (keyCount == 1)
    {
        // 키가 하나면 그대로 사용
        outT = keys[0].translation;
        outR = keys[0].rotationQuat;
        outS = keys[0].scale;
        return;
    }

    // timeSec을 키 범위 안으로 clamp
    float startTime = keys.front().timeSec;
    float endTime = keys.back().timeSec;
    if (timeSec <= startTime) timeSec = startTime;
    if (timeSec >= endTime)   timeSec = endTime;

    // timeSec이 들어갈 구간 [k0, k1]을 찾기
    size_t k1 = 1;
    for (; k1 < keyCount; ++k1)
    {
        if (keys[k1].timeSec >= timeSec)
            break;
    }
    if (k1 >= keyCount)
    {
        // safety: 마지막 키 사용
        outT = keys.back().translation;
        outR = keys.back().rotationQuat;
        outS = keys.back().scale;
        return;
    }

    size_t k0 = k1 - 1;
    const Keyframe& key0 = keys[k0];
    const Keyframe& key1 = keys[k1];

    float t0 = key0.timeSec;
    float t1 = key1.timeSec;
    float span = (t1 - t0);
    float alpha = (span > 0.0f) ? ((timeSec - t0) / span) : 0.0f;

    // 위치 / 스케일: 선형보간
    XMVECTOR T0 = XMLoadFloat3(&key0.translation);
    XMVECTOR T1 = XMLoadFloat3(&key1.translation);
    XMVECTOR S0 = XMLoadFloat3(&key0.scale);
    XMVECTOR S1 = XMLoadFloat3(&key1.scale);

    XMVECTOR T = XMVectorLerp(T0, T1, alpha);
    XMVECTOR S = XMVectorLerp(S0, S1, alpha);

    XMStoreFloat3(&outT, T);
    XMStoreFloat3(&outS, S);

    // 회전: 쿼터니언 SLERP
    XMVECTOR R0 = XMLoadFloat4(&key0.rotationQuat);
    XMVECTOR R1 = XMLoadFloat4(&key1.rotationQuat);
    XMVECTOR R = XMQuaternionSlerp(R0, R1, alpha);
    R = XMQuaternionNormalize(R);
    XMStoreFloat4(&outR, R);
}

// ============================================================
// AnimationClip::Evaluate
//   - timeSec 시각에서 각 본의 "로컬 행렬"을 outLocalTransforms 에 채움
//   - 키프레임이 없는 본은 skeleton[i].bindLocal 사용
// ============================================================
void AnimationClip::Evaluate(
    float timeSec,
    const std::vector<Bone>& skeleton,
    std::vector<XMFLOAT4X4>& outLocalTransforms) const
{
    const size_t trackCount = boneTracks.size();
    const size_t skeletonCount = skeleton.size();

    // 트랙 자체가 없으면 아무 것도 안 함
    if (trackCount == 0 || skeletonCount == 0)
    {
        outLocalTransforms.clear();
        return;
    }

    // 안전 차원에서 둘 중 작은 쪽만 사용
    const size_t boneCount = (trackCount < skeletonCount) ? trackCount : skeletonCount;

    if (outLocalTransforms.size() < boneCount)
        outLocalTransforms.resize(boneCount);

    for (size_t i = 0; i < boneCount; ++i)
    {
        const BoneKeyframes& track = boneTracks[i];

        // 이 본에 키프레임이 하나도 없으면 → bindLocal 사용
        if (track.keyframes.empty())
        {
            outLocalTransforms[i] = skeleton[i].bindLocal;
            continue;
        }

        // 키가 있는 본은 기존대로 TRS 보간
        XMFLOAT3 t;
        XMFLOAT4 r;
        XMFLOAT3 s;

        SampleBoneTrack(track.keyframes, timeSec, t, r, s);
        BuildTRSMatrix(t, r, s, outLocalTransforms[i]);
    }
}

