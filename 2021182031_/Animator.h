#pragma once
#include "stdafx.h"
#include "AnimatorData.h"

/*
    ============================================================
    CAnimator
    ------------------------------------------------------------
    - 스켈레톤 본 계층 정보를 보유
    - AnimationClip 여러 개를 관리
    - 매 프레임 Update(dt)에서 최종 본 행렬 계산
    - CMesh가 이 최종 행렬을 받아 GPU에 올림
    ============================================================
*/
class CAnimator
{
public:
    CAnimator() = default;
    ~CAnimator() = default;

public:
    // 스켈레톤(정적 본 목록)을 메시에서 받아오는 함수
    void SetSkeleton(const std::vector<Bone>& bones,
        const std::unordered_map<std::string, int>& boneNameToIndex);

    // 애니메이션 클립 등록
    void AddClip(const AnimationClip& clip);

    // 클립 재생 (이름으로 찾기)
    bool Play(const std::string& clipName, bool loop = true, float startTime = 0.0f);

    // 강제 시간 설정
    void SetTime(float timeSec);

    // 정지
    void Stop();

    // 매 프레임 애니메이션 진행 + 최종 본 행렬 계산
    void Update(float dt);

    // 현재 재생 중인 클립 이름 반환
    const std::string& GetCurrentClipName() const;

    // 최종 본 행렬 배열 반환 (CMesh가 GPU 업로드용으로 사용)
    const std::vector<XMFLOAT4X4>& GetFinalBoneMatrices() const;

    // 스켈레톤 본 개수
    int GetBoneCount() const { return (int)m_Skeleton.size(); }

    // 애니메이션이 존재하는지 확인
    bool HasClip(const std::string& name) const;

    void SetNextClipAfterEnd(const std::string& clip) { m_NextClipAfterEnd = clip; }
private:
    // 스켈레톤
    std::vector<Bone> m_Skeleton;   // Bone.name, parentIndex, offsetMatrix 등
    std::unordered_map<std::string, int> m_BoneNameToIndex;

    // 클립들
    std::unordered_map<std::string, AnimationClip> m_Clips;

    // 현재 재생 중인 클립
    AnimationClip* m_pCurrentClip = nullptr;

    // 재생 시간 / 상태
    float m_fCurrentTime = 0.0f;
    bool  m_bPlaying = false;
    bool  m_bLoop = true;

    // 최종 bone matrices (VS에서 gBoneTransforms로 직접 들어갈 형태)
    std::vector<XMFLOAT4X4> m_FinalBoneMatrices;

    // 내부 버퍼 (로컬/글로벌 트랜스폼 계산 시 사용)
    std::vector<XMFLOAT4X4> m_LocalPose;    // 각 본의 로컬 행렬
    std::vector<XMFLOAT4X4> m_GlobalPose;   // 각 본의 글로벌 행렬

    // Animator.h (선언만, 구현은 이후 단계)
    bool LoadClipFromFBXAndAdd(const char* filename,
        const std::string& clipName,
        float timeScale = 1.0f);

    string m_NextClipAfterEnd;

};