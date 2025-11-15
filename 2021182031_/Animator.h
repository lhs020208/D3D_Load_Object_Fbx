#pragma once
#include "stdafx.h"

// ------------------------------------------------------------
// 한 시점에서의 본 변환 (T + R + S)
// ------------------------------------------------------------
struct AnimationKey
{
    float time = 0.0f;               // 이 키프레임의 시간 (초 단위)

    XMFLOAT3 translation = { 0,0,0 };  // 위치
    XMFLOAT4 rotation = { 0,0,0,1 }; // 회전(쿼터니언)
    XMFLOAT3 scale = { 1,1,1 };   // 스케일
};

// ------------------------------------------------------------
// 특정 본(Bone)에 대한 모든 키프레임
// ------------------------------------------------------------
struct BoneKeyframes
{
    std::string boneName;                 // 본 이름
    int boneIndex = -1;                   // CMesh의 m_Bones 인덱스

    std::vector<AnimationKey> keys;       // 이 본을 위한 키프레임들

    // 키프레임이 없을 수도 있음 (스켈레톤 전체에는 존재하지만 애니메이션에는 없는 경우)
    bool HasKeys() const { return !keys.empty(); }
};

// ------------------------------------------------------------
// 하나의 Animation Clip (예: Idle, Run, Walk)
// ------------------------------------------------------------
struct AnimationClip
{
    std::string name;                     // 클립 이름 (FBX AnimStack 이름)

    float duration = 0.0f;                // 총 재생 시간 (초 단위)
    float frameRate = 30.0f;              // FBX에서 얻을 수 있는 기본 FPS (선택사항)

    std::vector<BoneKeyframes> boneTracks; // 각 본별 키프레임 트랙

    // boneIndex 기반으로 빠르게 찾기 위해
    BoneKeyframes* GetBoneTrack(int boneIndex)
    {
        for (auto& t : boneTracks)
            if (t.boneIndex == boneIndex) return &t;
        return nullptr;
    }
};
