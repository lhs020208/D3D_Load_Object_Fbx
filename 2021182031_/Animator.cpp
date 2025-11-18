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
    // 0) 재생 중이 아니면 아무 것도 안 함
    if (!m_bPlaying || !m_pCurrentClip)
        return;

    // 1) 시간 진행
    m_fCurrentTime += dt;

    float duration = m_pCurrentClip->duration;

    if (duration > 0.0f)
    {
        if (m_fCurrentTime > duration)
        {
            if (m_bLoop)
            {
                // 루프: duration 기준으로 감기
                m_fCurrentTime = fmodf(m_fCurrentTime, duration);
            }
            else
            {
                // 비루프: 끝에서 멈춤
                m_fCurrentTime = duration;
                m_bPlaying = false;
            }
        }
        else if (m_fCurrentTime < 0.0f)
        {
            m_fCurrentTime = 0.0f;
        }
    }

    const size_t boneCount = m_Skeleton.size();
    if (boneCount == 0)
        return;

    // 2) 버퍼 사이즈 보정
    if (m_LocalPose.size() < boneCount) m_LocalPose.resize(boneCount);
    if (m_GlobalPose.size() < boneCount) m_GlobalPose.resize(boneCount);
    if (m_FinalBoneMatrices.size() < boneCount) m_FinalBoneMatrices.resize(boneCount);

    // 3) 현재 시간에서 로컬 본 행렬들 샘플링
    //    (AnimationClip::Evaluate 가 TRS 보간해서 로컬 행렬을 outLocalTransforms에 채운다고 가정)
    m_pCurrentClip->Evaluate(m_fCurrentTime, m_LocalPose);

    using namespace DirectX;

    // 4) 로컬 → 글로벌 행렬 계산
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX localM = XMLoadFloat4x4(&m_LocalPose[i]);
        int parentIndex = m_Skeleton[i].parentIndex;

        if (parentIndex < 0 || parentIndex >= (int)boneCount)
        {
            // 루트본: 로컬 == 글로벌
            XMStoreFloat4x4(&m_GlobalPose[i], localM);
        }
        else
        {
            // 자식본: 글로벌 = 로컬 * 부모글로벌
            // (DirectXMath의 행벡터/우측 곱 컨벤션에 맞춘 순서)
            XMMATRIX parentGlobal = XMLoadFloat4x4(&m_GlobalPose[parentIndex]);
            XMMATRIX globalM = localM * parentGlobal;
            XMStoreFloat4x4(&m_GlobalPose[i], globalM);
        }
    }

    // 5) 최종 스키닝 행렬: Global * offsetMatrix(inverse bind pose)
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX globalM = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offsetM = XMLoadFloat4x4(&m_Skeleton[i].offsetMatrix);

        XMMATRIX skinM = globalM * offsetM;
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skinM);
    }
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
