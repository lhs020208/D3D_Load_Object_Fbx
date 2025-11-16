#include "stdafx.h"
#include "CAnimator.h"

CAnimator::CAnimator()
{
    m_bPlaying = false;
    m_bLoop = true;
    m_fCurrentTime = 0.0f;
    m_nCurrentClip = -1;
    m_nBones = 0;
}

CAnimator::~CAnimator()
{
}

// ------------------------------------------------------------
// 클립 관리
// ------------------------------------------------------------
void CAnimator::AddClip(const AnimationClip& clip)
{
    m_Clips.push_back(clip);
}

void CAnimator::SetClip(int clipIndex, bool loop)
{
    if (clipIndex < 0 || clipIndex >= (int)m_Clips.size())
        return;

    m_nCurrentClip = clipIndex;
    m_fCurrentTime = 0.0f;
    m_bLoop = loop;
    m_bPlaying = true;
}

void CAnimator::SetClip(const std::string& clipName, bool loop)
{
    for (int i = 0; i < (int)m_Clips.size(); ++i)
    {
        if (m_Clips[i].name == clipName)
        {
            SetClip(i, loop);
            return;
        }
    }
}

const AnimationClip* CAnimator::GetCurrentClip() const
{
    if (m_nCurrentClip < 0 || m_nCurrentClip >= (int)m_Clips.size())
        return nullptr;
    return &m_Clips[m_nCurrentClip];
}

// ------------------------------------------------------------
// 재생 컨트롤
// ------------------------------------------------------------
void CAnimator::Play()
{
    if (!m_Clips.empty() && m_nCurrentClip >= 0)
        m_bPlaying = true;
}

void CAnimator::Pause()
{
    m_bPlaying = false;
}

void CAnimator::Stop()
{
    m_bPlaying = false;
    m_fCurrentTime = 0.0f;
}

// ------------------------------------------------------------
// 본 정보 설정
// ------------------------------------------------------------
void CAnimator::SetBoneCount(int numBones)
{
    m_nBones = numBones;

    m_BoneOffsets.resize(m_nBones);
    m_BoneParents.resize(m_nBones, -1);
    m_FinalBoneMatrices.resize(m_nBones);
    m_LocalPose.resize(m_nBones);
    m_GlobalPose.resize(m_nBones);

    // 기본값 초기화
    for (int i = 0; i < m_nBones; ++i)
    {
        XMStoreFloat4x4(&m_BoneOffsets[i], XMMatrixIdentity());
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], XMMatrixIdentity());
        XMStoreFloat4x4(&m_LocalPose[i], XMMatrixIdentity());
        XMStoreFloat4x4(&m_GlobalPose[i], XMMatrixIdentity());
    }
}

void CAnimator::SetBoneOffsetMatrix(int boneIndex, const XMFLOAT4X4& offset)
{
    if (boneIndex < 0 || boneIndex >= m_nBones) return;
    m_BoneOffsets[boneIndex] = offset;
}

void CAnimator::SetBoneParent(int boneIndex, int parentIndex)
{
    if (boneIndex < 0 || boneIndex >= m_nBones) return;
    m_BoneParents[boneIndex] = parentIndex;
}

// ------------------------------------------------------------
// 프레임 갱신
// ------------------------------------------------------------
void CAnimator::Update(float dt)
{
    // 클립 / 본 없으면 아무것도 안 함
    if (m_Clips.empty() || m_nCurrentClip < 0 || m_nBones == 0)
        return;

    AnimationClip& clip = m_Clips[m_nCurrentClip];

    // 1) 시간 갱신
    if (m_bPlaying)
    {
        m_fCurrentTime += dt;

        if (m_fCurrentTime > clip.duration)
        {
            if (m_bLoop && clip.duration > 0.0f)
            {
                m_fCurrentTime = fmod(m_fCurrentTime, clip.duration);
            }
            else
            {
                m_fCurrentTime = clip.duration;
                m_bPlaying = false;
            }
        }
    }

    float t = m_fCurrentTime;
    size_t boneCount = static_cast<size_t>(m_nBones);

    // 2) 포즈 배열 크기 맞추기
    if (m_LocalPose.size() != boneCount)        m_LocalPose.resize(boneCount);
    if (m_GlobalPose.size() != boneCount)       m_GlobalPose.resize(boneCount);
    if (m_FinalBoneMatrices.size() != boneCount)m_FinalBoneMatrices.resize(boneCount);

    // 3) 기본 Local Pose를 단위행렬로 초기화
    for (size_t i = 0; i < boneCount; ++i)
        XMStoreFloat4x4(&m_LocalPose[i], XMMatrixIdentity());

    // 4) BoneTrack에서 현재 시간 t에 해당하는 Local Pose 채우기
    for (auto& track : clip.boneTracks)
    {
        int boneIndex = track.boneIndex;
        if (boneIndex < 0 || boneIndex >= (int)boneCount) continue;

        m_LocalPose[boneIndex] = InterpolateBoneMatrix(track, t);
    }

    // 5) 부모-자식 계층 따라 Global Pose 계산
    for (size_t i = 0; i < boneCount; ++i)
    {
        int parent = m_BoneParents[i];

        if (parent < 0)
        {
            // 루트 본: Global = Local
            m_GlobalPose[i] = m_LocalPose[i];
        }
        else
        {
            // 자식 본: Global = ParentGlobal * Local
            m_GlobalPose[i] = CombineParentChild(m_GlobalPose[parent], m_LocalPose[i]);
        }
    }

    // 6) Skin Matrix = Global * Offset  (transpose는 여기서 하지 않는다!)
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_BoneOffsets[i]); // offset = boneBind^-1 * meshBind

        XMMATRIX skin = global * offset; // v' = v * global * offset

        // 최종 스킨 행렬을 그대로 저장 (Transpose는 CMesh::UpdateBoneConstantBuffer에서 처리)
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
    }
}


// ------------------------------------------------------------
// private helper 들
// ------------------------------------------------------------
XMFLOAT4X4 CAnimator::InterpolateBoneMatrix(const BoneKeyframes& track, float time)
{
    XMFLOAT4X4 result;
    XMStoreFloat4x4(&result, XMMatrixIdentity());

    if (track.keys.empty())
        return result;

    // 키가 하나면 그대로 사용
    if (track.keys.size() == 1)
    {
        const AnimationKey& k = track.keys[0];

        XMMATRIX S = XMMatrixScaling(k.scale.x, k.scale.y, k.scale.z);
        XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&k.rotation));
        XMMATRIX T = XMMatrixTranslation(k.translation.x, k.translation.y, k.translation.z);

        XMStoreFloat4x4(&result, S * R * T);
        return result;
    }

    // time 이 위치하는 구간 찾기
    int k1 = 0, k2 = 1;
    for (int i = 0; i < (int)track.keys.size() - 1; ++i)
    {
        if (time >= track.keys[i].time && time <= track.keys[i + 1].time)
        {
            k1 = i;
            k2 = i + 1;
            break;
        }
    }

    const AnimationKey& A = track.keys[k1];
    const AnimationKey& B = track.keys[k2];

    float segment = (B.time - A.time);
    float lerp = (segment > 1e-8f) ? (time - A.time) / segment : 0.0f;
    if (lerp < 0.0f) lerp = 0.0f;
    if (lerp > 1.0f) lerp = 1.0f;

    // Translation
    XMFLOAT3 Tinterp{
        A.translation.x + (B.translation.x - A.translation.x) * lerp,
        A.translation.y + (B.translation.y - A.translation.y) * lerp,
        A.translation.z + (B.translation.z - A.translation.z) * lerp
    };

    // Scale
    XMFLOAT3 Sinterp{
        A.scale.x + (B.scale.x - A.scale.x) * lerp,
        A.scale.y + (B.scale.y - A.scale.y) * lerp,
        A.scale.z + (B.scale.z - A.scale.z) * lerp
    };

    // Rotation
    XMVECTOR qA = XMLoadFloat4(&A.rotation);
    XMVECTOR qB = XMLoadFloat4(&B.rotation);
    XMVECTOR qR = XMQuaternionSlerp(qA, qB, lerp);

    XMMATRIX Sm = XMMatrixScaling(Sinterp.x, Sinterp.y, Sinterp.z);
    XMMATRIX Rm = XMMatrixRotationQuaternion(qR);
    XMMATRIX Tm = XMMatrixTranslation(Tinterp.x, Tinterp.y, Tinterp.z);

    XMStoreFloat4x4(&result, Sm * Rm * Tm);
    return result;
}

XMFLOAT4X4 CAnimator::CombineParentChild(const XMFLOAT4X4& parent, const XMFLOAT4X4& local)
{
    XMMATRIX P = XMLoadFloat4x4(&parent); // parentGlobal
    XMMATRIX L = XMLoadFloat4x4(&local);  // childLocal
    XMFLOAT4X4 out;
    XMStoreFloat4x4(&out, P * L);         // parentGlobal * childLocal
    return out;
}
