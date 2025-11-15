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
    if (m_Clips.empty() || m_nCurrentClip < 0 || m_nBones == 0)
        return;

    AnimationClip& clip = m_Clips[m_nCurrentClip];

    // 1) 시간 진행 (재생 중일 때만)
    if (m_bPlaying)
    {
        m_fCurrentTime += dt;

        if (m_fCurrentTime > clip.duration)
        {
            if (m_bLoop && clip.duration > 0.0f)
                m_fCurrentTime = fmod(m_fCurrentTime, clip.duration);
            else
                m_fCurrentTime = clip.duration;
        }
    }

    float t = m_fCurrentTime;

    size_t boneCount = (size_t)m_nBones;

    // 2) Local / Global / Final 배열 크기 확보
    m_LocalPose.resize(boneCount);
    m_GlobalPose.resize(boneCount);
    m_FinalBoneMatrices.resize(boneCount);

    // 기본값은 Identity
    for (size_t i = 0; i < boneCount; ++i)
        XMStoreFloat4x4(&m_LocalPose[i], XMMatrixIdentity());

    // 3) BoneTrack 별 키프레임 보간 → Local Pose 생성
    for (auto& track : clip.boneTracks)
    {
        int boneIndex = track.boneIndex;
        if (boneIndex < 0 || boneIndex >= (int)boneCount) continue;

        m_LocalPose[boneIndex] = InterpolateBoneMatrix(track, t);
    }

    // 4) 계층을 따라 Global Pose 계산
    for (size_t i = 0; i < boneCount; ++i)
    {
        int parent = m_BoneParents[i];
        XMFLOAT4X4 local = m_LocalPose[i];

        if (parent < 0)
            m_GlobalPose[i] = local;
        else
            m_GlobalPose[i] = CombineParentChild(m_GlobalPose[parent], local);
    }

    // 5) SkinMatrix = Offset * Global
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_BoneOffsets[i]);

        XMMATRIX skin = offset * global;
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
    XMMATRIX P = XMLoadFloat4x4(&parent);
    XMMATRIX L = XMLoadFloat4x4(&local);
    XMFLOAT4X4 out;
    XMStoreFloat4x4(&out, L * P);   // childLocal * parentGlobal
    return out;
}
