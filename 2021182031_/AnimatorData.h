#pragma once
#include "stdafx.h"

/*
    ============================================================
    Keyframe
    ------------------------------------------------------------
    - 특정 시각(timeSec)에서의 단일 본 정보(TRS)
    - Rotation은 쿼터니언
    ============================================================
*/
struct Keyframe
{
    float       timeSec = 0.0f;     // 해당 키프레임의 시간(초)
    XMFLOAT3    translation = { 0.f, 0.f, 0.f };
    XMFLOAT4    rotationQuat = { 0.f, 0.f, 0.f, 1.f };
    XMFLOAT3    scale = { 1.f, 1.f, 1.f };
};

/*
    ============================================================
    BoneKeyframes
    ------------------------------------------------------------
    - 특정 본 1개가 갖는 전체 키프레임 목록
    - FBX 애니에서 본 이름으로 식별
    ============================================================
*/
struct BoneKeyframes
{
    std::string         boneName;           // 본 이름
    int                 boneIndex = -1;     // CMesh의 m_Bones 안에서의 인덱스
    std::vector<Keyframe> keyframes;        // 시간순 정렬된 키프레임

    // 키프레임이 하나도 없다면 이 본은 정적(기본 자세)로 취급
};

/*
    ============================================================
    AnimationClip
    ------------------------------------------------------------
    - 하나의 애니메이션 파일(예: Idle, Walk, Jump)에 해당
    - 본별 BoneKeyframes 배열로 구성됨
    - Evaluate(t): t초에 대해 본마다 로컬 TRS 행렬을 만들어낼 예정
    ============================================================
*/
struct AnimationClip
{
    std::string name;            // 클립 이름 (예: "Idle", "Walk", "Jump")
    float       duration = 0.f;  // 전체 길이(초), FBX에서 자동 계산 예정

    // 본 인덱스 = CMesh.m_Bones의 인덱스와 동일한 위치에 저장
    // 즉, m_BoneTracks[i]가 i번째 본의 트랙.
    std::vector<BoneKeyframes> boneTracks;

    // 이름 → 본트랙 인덱스 (필수는 아니지만 편의용)
    std::unordered_map<std::string, int> boneNameToTrack;

    void Evaluate(float timeSec, std::vector<XMFLOAT4X4>& outLocalTransforms) const;
};

struct Bone
{
    std::string name;            // 본 이름
    int parentIndex;             // 부모 본 인덱스
    // inverse bind pose (모델 공간 → 본 공간)
    // 나중에 FBX Skin(Cluster)에서 실제 값을 채울 예정.
    XMFLOAT4X4 offsetMatrix;     // Inverse Bind Pose (모델 공간 → 본 공간)
};

struct SkinnedVertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
    UINT boneIndices[4];     // 어떤 본들이 영향을 주는가
    float boneWeights[4];    // 각 본의 영향 비율
};