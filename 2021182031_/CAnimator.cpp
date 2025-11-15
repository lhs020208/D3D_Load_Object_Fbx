#include "stdafx.h"
#include "CAnimator.h"

void CAnimator::Update(float dt)
{
    if (m_Clips.empty() || m_nCurrentClip < 0) return;
    AnimationClip& clip = m_Clips[m_nCurrentClip];

    // -----------------------------------------
    // 1) 시간 진행
    // -----------------------------------------
    m_fCurrentTime += dt;

    if (m_fCurrentTime > clip.duration)
    {
        if (m_bLoop)
            m_fCurrentTime = fmod(m_fCurrentTime, clip.duration);
        else
            m_fCurrentTime = clip.duration;
    }

    float t = m_fCurrentTime;

    // -----------------------------------------
    // 2) 본 개수만큼 Local Pose 초기화
    // -----------------------------------------
    size_t boneCount = (size_t)m_nBones;
    if (boneCount == 0) return;

    m_LocalPose.resize(boneCount);
    m_GlobalPose.resize(boneCount);
    m_FinalBoneMatrices.resize(boneCount);

    for (size_t i = 0; i < boneCount; ++i)
        XMStoreFloat4x4(&m_LocalPose[i], XMMatrixIdentity());

    // -----------------------------------------
    // 3) 각 BoneTrack에서 보간 → Local Pose 생성
    // -----------------------------------------
    for (auto& track : clip.boneTracks)
    {
        int boneIndex = track.boneIndex;
        if (boneIndex < 0 || boneIndex >= (int)boneCount) continue;

        // 키가 1개인 경우
        if (track.keys.size() == 1)
        {
            const AnimationKey& k = track.keys[0];

            XMMATRIX S = XMMatrixScaling(k.scale.x, k.scale.y, k.scale.z);
            XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&k.rotation));
            XMMATRIX T = XMMatrixTranslation(k.translation.x, k.translation.y, k.translation.z);

            XMStoreFloat4x4(&m_LocalPose[boneIndex], S * R * T);
            continue;
        }

        // 보간 구간 찾기
        int k1 = 0, k2 = 0;

        for (int i = 0; i < (int)track.keys.size() - 1; ++i)
        {
            if (t >= track.keys[i].time && t <= track.keys[i + 1].time)
            {
                k1 = i;
                k2 = i + 1;
                break;
            }
        }

        const AnimationKey& A = track.keys[k1];
        const AnimationKey& B = track.keys[k2];

        float segment = (B.time - A.time);
        float lerp = (segment > 1e-8f) ? (t - A.time) / segment : 0.0f;

        // ---- Translation ----
        XMFLOAT3 Tinterp{
            A.translation.x + (B.translation.x - A.translation.x) * lerp,
            A.translation.y + (B.translation.y - A.translation.y) * lerp,
            A.translation.z + (B.translation.z - A.translation.z) * lerp
        };

        // ---- Scale ----
        XMFLOAT3 Sinterp{
            A.scale.x + (B.scale.x - A.scale.x) * lerp,
            A.scale.y + (B.scale.y - A.scale.y) * lerp,
            A.scale.z + (B.scale.z - A.scale.z) * lerp
        };

        // ---- Rotation (Quat Slerp) ----
        XMVECTOR qA = XMLoadFloat4(&A.rotation);
        XMVECTOR qB = XMLoadFloat4(&B.rotation);
        XMVECTOR qR = XMQuaternionSlerp(qA, qB, lerp);

        XMMATRIX Sm = XMMatrixScaling(Sinterp.x, Sinterp.y, Sinterp.z);
        XMMATRIX Rm = XMMatrixRotationQuaternion(qR);
        XMMATRIX Tm = XMMatrixTranslation(Tinterp.x, Tinterp.y, Tinterp.z);

        XMStoreFloat4x4(&m_LocalPose[boneIndex], Sm * Rm * Tm);
    }

    // -----------------------------------------
    // 4) 본 계층 → Global Pose 계산
    // -----------------------------------------
    for (size_t i = 0; i < boneCount; ++i)
    {
        int parent = m_BoneParents[i];

        XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

        if (parent < 0)
        {
            XMStoreFloat4x4(&m_GlobalPose[i], local);
        }
        else
        {
            XMMATRIX parentM = XMLoadFloat4x4(&m_GlobalPose[parent]);
            XMStoreFloat4x4(&m_GlobalPose[i], local * parentM);
        }
    }

    // -----------------------------------------
    // 5) SkinMatrix = Offset * Global
    // -----------------------------------------
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_BoneOffsets[i]);

        XMMATRIX skin = offset * global;

        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
    }
}
