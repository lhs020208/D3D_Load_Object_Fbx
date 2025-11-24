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
//   - 본 행렬 계산
// ============================================================
void CAnimator::Update(float dt)
{
    if (!m_bPlaying || !m_pCurrentClip)
        return;
	
    // 시간 증가
    //m_fCurrentTime += dt;
    m_fCurrentTime += 0.001f;


    if (m_fCurrentTime > m_pCurrentClip->duration)
    {
        if (m_bLoop) m_fCurrentTime = fmod(m_fCurrentTime, m_pCurrentClip->duration);
        else m_fCurrentTime = m_pCurrentClip->duration;
    }

    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0) return;

    
    // 1) 로컬 포즈 계산
    m_pCurrentClip->Evaluate(
        m_fCurrentTime,
        m_Skeleton,
        m_LocalPose
    );
    

    // 2) 글로벌 포즈 계산 (부모-자식 연결)
    
    for (int i = 0; i < boneCount; ++i)
    {
        int parent = m_Skeleton[i].parentIndex;

        XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

        if (parent < 0)
        {
            // 루트
            XMStoreFloat4x4(&m_GlobalPose[i], local);
        }
        else
        {
            XMMATRIX parentM = XMLoadFloat4x4(&m_GlobalPose[parent]);
            XMMATRIX global = local * parentM;
            XMStoreFloat4x4(&m_GlobalPose[i], global);
        }
    }
    
    
    // 3) 최종 본 행렬 = offsetMatrix * globalTransform
    for (int i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_Skeleton[i].offsetMatrix);

        XMMATRIX skin = offset * global;
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
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
