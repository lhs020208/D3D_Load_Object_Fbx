#pragma once
#include "Animator.h"   // AnimationKey, BoneKeyframes, AnimationClip 구조체 포함

class CAnimator
{
public:
    CAnimator();
    ~CAnimator();

    // ============================
    // 애니메이션 클립 관리
    // ============================
    void AddClip(const AnimationClip& clip);
    void SetClip(int clipIndex, bool loop = true);            // 인덱스로 선택
    void SetClip(const std::string& clipName, bool loop = true); // 이름으로 선택

    int  GetCurrentClipIndex() const { return m_nCurrentClip; }
    const AnimationClip* GetCurrentClip() const;

    // ============================
    // 재생 제어
    // ============================
    void Play();
    void Pause();
    void Stop();
    void SetLoop(bool loop) { m_bLoop = loop; }

    bool IsPlaying() const { return m_bPlaying; }
    bool IsLooping() const { return m_bLoop; }

    // ============================
    // 매 프레임 업데이트
    // ============================
    void Update(float dt);  // dt: FrameAdvance → Scene → GameObject → Mesh에서 호출

    // ============================
    // 최종 본 변환 행렬(스킨 행렬) 계산 결과 반환
    // ============================
    // CMesh::m_pxmf4x4BoneTransforms에 그대로 복사 가능
    const std::vector<XMFLOAT4X4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

    // 본 개수 지정 (CMesh에서 스켈레톤 로드 후 설정)
    void SetBoneCount(int numBones);

    // 본의 글로벌 바인드포즈 역행렬(OffsetMatrix) 등록
    // CMesh::m_Bones[i].offsetMatrix에서 가져와야 함
    void SetBoneOffsetMatrix(int boneIndex, const XMFLOAT4X4& offset);

    // 본 계층 구조 정보 등록
    void SetBoneParent(int boneIndex, int parentIndex);

private:
    // ============================
    // 내부 계산용 함수(정의만; 구현은 cpp)
    // ============================
    XMFLOAT4X4 InterpolateBoneMatrix(const BoneKeyframes& track, float time);
    XMFLOAT4X4 CombineParentChild(const XMFLOAT4X4& parent, const XMFLOAT4X4& local);

private:
    // ============================
    // 애니메이션 상태
    // ============================
    bool m_bPlaying = false;
    bool m_bLoop = true;
    float m_fCurrentTime = 0.0f;   // 현재 재생 시간(초)

    int m_nCurrentClip = -1;       // 현재 재생 중인 클립
    std::vector<AnimationClip> m_Clips;

    // ============================
    // 본 트랜스폼 계산용
    // ============================
    int m_nBones = 0;

    // 본의 Offset(Inverse Bind Pose) 행렬
    std::vector<XMFLOAT4X4> m_BoneOffsets;

    // 본의 부모 인덱스 (스켈레톤 계층 구조)
    std::vector<int> m_BoneParents;

    // 최종 계산된 본 행렬들 (Skinning용)
    // RootSignature b4에서 그대로 GPU로 보냄
    std::vector<XMFLOAT4X4> m_FinalBoneMatrices;
};
