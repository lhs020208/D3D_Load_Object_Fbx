#include "stdafx.h"
#include "Animator.h"

// ============================================================
// SetSkeleton
//   - 메시에서 본 계층과 offsetMatrix를 가져와 저장
//   - 본 개수에 맞춰 내부 버퍼 크기 초기화
// ============================================================
void CAnimator::SetSkeleton(const std::vector<Bone>& bones,
    const std::unordered_map<std::string, int>& boneNameToIndex)
{
    m_Skeleton = bones;
    m_BoneNameToIndex = boneNameToIndex;

    int boneCount = (int)bones.size();

    // 내부 포즈/최종 행렬 버퍼 크기 재설정
    m_LocalPose.resize(boneCount);
    m_GlobalPose.resize(boneCount);
    m_FinalBoneMatrices.resize(boneCount);

    // 기본값: identity
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    for (int i = 0; i < boneCount; ++i)
    {
        m_LocalPose[i] = identity;
        m_GlobalPose[i] = identity;
        m_FinalBoneMatrices[i] = identity;
    }
}

// ============================================================
// AddClip
//   - 애니메이션 클립 등록
// ============================================================
void CAnimator::AddClip(const AnimationClip& clip)
{
    m_Clips[clip.name] = clip;
}

// ============================================================
// HasClip
// ============================================================
bool CAnimator::HasClip(const std::string& name) const
{
    return (m_Clips.find(name) != m_Clips.end());
}

// ============================================================
// Play
//   - 클립 재생 시작
// ============================================================
bool CAnimator::Play(const std::string& clipName, bool loop, float startTime)
{
    auto it = m_Clips.find(clipName);
    if (it == m_Clips.end())
        return false;

    m_pCurrentClip = &it->second;
    m_fCurrentTime = startTime;
    m_bLoop = loop;
    m_bPlaying = true;

    return true;
}

// ============================================================
// Stop
// ============================================================
void CAnimator::Stop()
{
    m_bPlaying = false;
    m_fCurrentTime = 0.0f;
}

// ============================================================
// SetTime
//   - 외부에서 강제로 재생 시간을 지정
// ============================================================
void CAnimator::SetTime(float timeSec)
{
    m_fCurrentTime = timeSec;
}

// ============================================================
// Update
//   - dt만큼 시간 증가
//   - 클립 범위 벗어나면 loop 처리
//   - 실제 본 행렬 계산은 TODO
// ============================================================
void CAnimator::Update(float dt)
{
    if (!m_bPlaying || !m_pCurrentClip)
        return;

    // 1) 시간 진행
    m_fCurrentTime += dt;

    // 클립 길이 확인
    float duration = m_pCurrentClip->duration;

    if (duration > 0.0f)
    {
        if (m_fCurrentTime > duration)
        {
            if (m_bLoop)
                m_fCurrentTime = fmod(m_fCurrentTime, duration);
            else
            {
                m_fCurrentTime = duration;
                m_bPlaying = false;
            }
        }
    }

    // 2) TODO: m_LocalPose = EvaluateLocalPose(m_fCurrentTime)
    //    - 각 본에 대해 키프레임 보간해서 로컬 TRS 행렬 생성
    //    - 현재는 전부 identity

    // 3) TODO: m_GlobalPose = LocalToGlobal(m_LocalPose, m_Skeleton)
    //    - 본 계층(parentIndex) 따라 글로벌 행렬 계산

    // 4) TODO: m_FinalBoneMatrices[i] = m_GlobalPose[i] * m_Skeleton[i].offsetMatrix
    //    - 스키닝에 사용할 최종 행렬

    // 현재는 임시로 identity 유지
}

// ============================================================
// GetCurrentClipName
// ============================================================
const std::string& CAnimator::GetCurrentClipName() const
{
    static std::string empty = "";
    if (!m_pCurrentClip) return empty;
    return m_pCurrentClip->name;
}

// ============================================================
// GetFinalBoneMatrices
//   - CMesh가 GPU CBV 업데이트에 사용
// ============================================================
const std::vector<XMFLOAT4X4>& CAnimator::GetFinalBoneMatrices() const
{
    return m_FinalBoneMatrices;
}
