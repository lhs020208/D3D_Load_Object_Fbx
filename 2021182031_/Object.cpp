//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "GameFramework.h"
#include "Animator.h"

inline float RandF(float fMin, float fMax)
{
	return(fMin + ((float)rand() / (float)RAND_MAX) * (fMax - fMin));
}

XMVECTOR RandomUnitVectorOnSphere()
{
	XMVECTOR xmvOne = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
	XMVECTOR xmvZero = XMVectorZero();

	while (true)
	{
		XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);
		if (!XMVector3Greater(XMVector3LengthSq(v), xmvOne)) return(XMVector3Normalize(v));
	}
}

CGameObject::CGameObject(int nMeshes)
{
	m_xmf4x4World = Matrix4x4::Identity();
	m_nMeshes = nMeshes;
	m_ppMeshes = NULL;
	if (m_nMeshes > 0)
	{
		m_ppMeshes = new CMesh * [m_nMeshes];
		for (int i = 0; i < m_nMeshes; i++) m_ppMeshes[i] = NULL;
	}
}

CGameObject::~CGameObject()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Release();
			m_ppMeshes[i] = NULL;
		}
		delete[] m_ppMeshes;
	}
	if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->Release();
	}
}

void CGameObject::SetMesh(int nIndex, CMesh* pMesh)
{
	if (m_ppMeshes && nIndex >= 0 && nIndex < m_nMeshes) {
		if (m_ppMeshes[nIndex]) m_ppMeshes[nIndex]->Release();
		m_ppMeshes[nIndex] = pMesh;
		if (pMesh) pMesh->AddRef();
	}
}

void CGameObject::SetShader(CShader *pShader)
{
	if (m_pShader) m_pShader->Release();
	m_pShader = pShader;
	if (m_pShader) m_pShader->AddRef();
}

void CGameObject::Animate(float fTimeElapsed)
{
	// 메시가 없으면 할 일 없음
	if (!m_ppMeshes || m_nMeshes <= 0) return;

	for (int i = 0; i < m_nMeshes; ++i)
	{
		CMesh* pMesh = m_ppMeshes[i];
		if (!pMesh) continue;

		// 스키닝 메시가 아니면 애니메이션 처리 안 함
		if (!pMesh->IsSkinnedMesh()) continue;

		// 애니메이터 확보(없으면 생성 + 스켈레톤 연결)
		CAnimator* pAnimator = pMesh->EnsureAnimator();
		if (!pAnimator) continue;

		// 1) 애니메이터 업데이트 (현재 클립 시간 진행 + 본 행렬 계산)
		pAnimator->Update(fTimeElapsed);

		// 2) 최종 본 행렬들을 가져와서 CBV에 업로드
		const auto& finalMats = pAnimator->GetFinalBoneMatrices();
		if (!finalMats.empty())
		{
			// 현재 구현에서는 cmdList를 쓰지 않으므로 nullptr 전달해도 됨
			pMesh->UpdateBoneTransformsOnGPU(
				nullptr,
				finalMats.data(),
				static_cast<int>(finalMats.size()));
		}
	}
}


void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);

	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 3, &m_xmf3Color, 16);
}
void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(pxmf4x4World)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4World, 0);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 3, &m_xmf3Color, 16);
}

void CGameObject::ReleaseShaderVariables()
{
}

void CGameObject::Render(ID3D12GraphicsCommandList* cmdList, CCamera* pCamera, CScene* pScene)
{
	OnPrepareRender();
	UpdateShaderVariables(cmdList);

	if (m_pShader)
		m_pShader->Render(cmdList, pCamera);

	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			CMesh* pMesh = m_ppMeshes[i];
			if (!pMesh) continue;

			// 스키닝 메시이면 b4에 본 행렬 CBV 바인딩
			if (pMesh->IsSkinnedMesh() && pMesh->HasBoneCB())
			{
				D3D12_GPU_VIRTUAL_ADDRESS boneCB = pMesh->GetBoneCBAddress();
				if (boneCB)
					cmdList->SetGraphicsRootConstantBufferView(4, boneCB);
			}

			// 실제 메시 렌더
			pMesh->Render(cmdList);
		}
	}
}

void CGameObject::SetSrvDescriptorInfo(ID3D12DescriptorHeap* heap, UINT inc)
{
	m_pd3dSrvDescriptorHeap = heap;
	m_nSrvDescriptorIncrementSize = inc;

	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; ++i)
		{
			if (m_ppMeshes[i])
				m_ppMeshes[i]->SetSrvDescriptorInfo(heap, inc);
		}
	}
}

void CGameObject::ReleaseUploadBuffers()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->ReleaseUploadBuffers();
		}
	}
}

void CGameObject::SetPosition(float x, float y, float z)
{
	m_xmf4x4World._41 = x;
	m_xmf4x4World._42 = y;
	m_xmf4x4World._43 = z;
}

void CGameObject::SetPosition(XMFLOAT3 xmf3Position)
{
	SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}

XMFLOAT3 CGameObject::GetPosition()
{
	return(XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43));
}

XMFLOAT3 CGameObject::GetLook()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33)));
}

XMFLOAT3 CGameObject::GetUp()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23)));
}

XMFLOAT3 CGameObject::GetRight()
{
	return(Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13)));
}


void CGameObject::MoveStrafe(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Right = GetRight();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Right, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Up = GetUp();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Up, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Look = GetLook();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Look, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	XMMATRIX mtxRotate = XMMatrixRotationRollPitchYaw(XMConvertToRadians(fPitch), XMConvertToRadians(fYaw), XMConvertToRadians(fRoll));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::Rotate(XMFLOAT3 *pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis), XMConvertToRadians(fAngle));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::SetScale(float x, float y, float z)
{
	m_xmf3Scale = XMFLOAT3(x, y, z);
}

void CGameObject::LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosition(), xmf3LookTo, xmf3Up);
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}

void CGameObject::LookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookAtLH(GetPosition(), xmf3LookAt, xmf3Up);
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}

void CGameObject::UpdateBoundingBox()
{
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->m_xmOOBB.Transform(m_xmOOBB, XMLoadFloat4x4(&m_xmf4x4World));
		}
		XMStoreFloat4(&m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&m_xmOOBB.Orientation)));
	}
}

int CGameObject::PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance)
{
	XMMATRIX xmmtxWorld = XMLoadFloat4x4(&m_xmf4x4World);
	XMMATRIX xmmtxWorldInv = XMMatrixInverse(nullptr, xmmtxWorld);

	// 뷰 행렬 역행렬을 사용해서 Ray Origin (카메라 위치) 추출
	XMMATRIX xmmtxViewInv = XMMatrixInverse(nullptr, xmmtxView);
	XMVECTOR xmvPickRayOrigin = xmmtxViewInv.r[3]; // 카메라 위치

	// Pick Ray의 방향을 World 좌표계 기준으로 변환
	XMVECTOR xmvPickRayDir = XMVector3TransformNormal(xmvPickPosition, xmmtxViewInv);
	xmvPickRayDir = XMVector3Normalize(xmvPickRayDir);

	// 로컬 공간으로 변환
	xmvPickRayOrigin = XMVector3TransformCoord(xmvPickRayOrigin, xmmtxWorldInv);
	xmvPickRayDir = XMVector3TransformNormal(xmvPickRayDir, xmmtxWorldInv);
	xmvPickRayDir = XMVector3Normalize(xmvPickRayDir);

	if (m_ppMeshes) 
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) return m_ppMeshes[i]->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDir, pfHitDistance);
		}
	return 0;
}
void CGameObject::GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection)
{
	XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&m_xmf4x4World) * xmmtxView);

	XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
	xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
	xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
	xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
}
void CGameObject::SetRotationTransform(XMFLOAT4X4* pmxf4x4Transform)
{
	m_xmf4x4World._11 = pmxf4x4Transform->_11; m_xmf4x4World._12 = pmxf4x4Transform->_12; m_xmf4x4World._13 = pmxf4x4Transform->_13;
	m_xmf4x4World._21 = pmxf4x4Transform->_21; m_xmf4x4World._22 = pmxf4x4Transform->_22; m_xmf4x4World._23 = pmxf4x4Transform->_23;
	m_xmf4x4World._31 = pmxf4x4Transform->_31; m_xmf4x4World._32 = pmxf4x4Transform->_32; m_xmf4x4World._33 = pmxf4x4Transform->_33;
}

void CGameObject::PlayAnimation(const std::string& clipName, bool loop, float start)
{
	if (!m_ppMeshes) return;

	for (int i = 0; i < m_nMeshes; ++i)
	{
		CMesh* mesh = m_ppMeshes[i];
		if (!mesh) continue;

		CAnimator* anim = mesh->GetAnimator();
		if (!anim) continue;

		// ============================================================
        // ★ mesh / anim 비교 출력
        // ============================================================
		/*
        {
            char buf[512];

            sprintf_s(buf,
                "[PlayAnimation] Mesh %d\n"
                "  Mesh ptr : %p\n"
                "  Animator : %p\n"
                "  BoneCount(mesh) = %d, BoneCount(anim) = %d\n",
                i,
                mesh,
                anim,
                mesh->GetBoneCount(),
                anim->GetBoneCount()
            );
            OutputDebugStringA(buf);

            // 본 이름도 비교 출력 (최대 20개까지만)
            const auto& bones = mesh->GetBones();
            int bc = (int)bones.size();

            for (int bi = 0; bi < bc && bi < 20; ++bi)
            {
                const auto& b = bones[bi];
                sprintf_s(buf,
                    "    Bone[%d]: %s (parent=%d)\n",
                    bi, b.name.c_str(), b.parentIndex);
                OutputDebugStringA(buf);
            }
        }
		*/
        // ============================================================

		if (!anim->Play(clipName, loop, start))
			continue;

		// ★ Play() 안에서 이미 m_FinalBoneMatrices는
		//    startTime 시점 포즈로 계산된 상태임

		const auto& mats = anim->GetFinalBoneMatrices();
		if (!mats.empty() && mesh->IsSkinnedMesh() && mesh->HasBoneCB())
		{
			mesh->UpdateBoneTransformsOnGPU(
				nullptr,
				mats.data(),
				static_cast<int>(mats.size()));
		}
	}
}


void CGameObject::SetNextAnimation(const std::string& clip)
{
	if (!m_ppMeshes) return;
	for (int i = 0; i < m_nMeshes; ++i)
	{
		CAnimator* anim = m_ppMeshes[i]->GetAnimator();
		if (!anim) return;

		anim->SetNextClipAfterEnd(clip);
	}
}