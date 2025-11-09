//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Object.h"
#include "Shader.h"
#include "GameFramework.h"

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

void CGameObject::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	OnPrepareRender();

	UpdateShaderVariables(pd3dCommandList);
	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Render(pd3dCommandList);
		}
	}
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, XMFLOAT4X4* pxmf4x4World)
{
	OnPrepareRender();
	UpdateShaderVariables(pd3dCommandList, pxmf4x4World);

	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);
	if (m_ppMeshes)
	{
		for (int i = 0; i < m_nMeshes; i++)
		{
			if (m_ppMeshes[i]) m_ppMeshes[i]->Render(pd3dCommandList);
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CCubeObject::Animate(float fElapsedTime)
{
}
void CCubeObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	CGameObject::Render(pd3dCommandList, pCamera);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CExplosionObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
		for (int i = 0; i < EXPLOSION_DEBRISES; i++) {
			if (Draw[i])
				CGameObject::Render(pd3dCommandList, pCamera, &m_pxmf4x4Transforms[i]);
		}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTankObject::Animate(float fElapsedTime)
{
	if (IsExist()) {
		if (m_bBlowingUp)
		{
			m_fElapsedTimes += fElapsedTime;
			if (m_fElapsedTimes >= m_fDuration)
			{
				SetExist(false);
				return;
			}

			for (int i = 0; i < EXPLOSION_DEBRISES; i++) {
				XMFLOAT3 direction = m_pxmf3SphereVectors[i];
				XMFLOAT3 position = Vector3::Add(GetPosition(), Vector3::ScalarProduct(direction, m_fElapsedTimes * m_fExplosionSpeed));
				XMFLOAT4X4 world = Matrix4x4::RotationAxis(direction, m_fElapsedTimes * XMConvertToRadians(m_fExplosionRotation));
				world._41 = position.x; world._42 = position.y; world._43 = position.z;
				m_pxmf4x4Transforms[i] = world;
			}
		}
		else
		{
			int forward_Step = 100;
			if (timer < forward_Step)
				SetPosition(Vector3::Add(GetPosition(), Vector3::ScalarProduct(GetLook(), fElapsedTime * 0.5f * 5, false)));
			if (timer >= forward_Step && timer < forward_Step + 30)
				Rotate(0.0f, 6.0f, 0.0f);
			if (timer >= forward_Step + 30 && timer < 2 * forward_Step + 30)
				SetPosition(Vector3::Add(GetPosition(), Vector3::ScalarProduct(GetLook(), fElapsedTime * 0.5f * 5, false)));
			if (timer >= 2 * forward_Step + 30 && timer < 2 * forward_Step + 60)
				Rotate(0.0f, 6.0f, 0.0f);

			timer++;
			if (timer == 2 * forward_Step + 60) timer = 0;

			if (shot) {
				bullet_timer++;
				bullet->SetPosition(Vector3::Add(bullet->GetPosition(), Vector3::ScalarProduct(bullet->GetLook(), fElapsedTime * 2.0f * 5, false)));
				if (bullet_timer >= 200) {
					bullet_timer = 0;
					SwitchShot();
				}
			}
			else {
				using namespace std;
				default_random_engine dre{ random_device{}() };
				uniform_int_distribution<int> uid{ 0,100 };
				if (uid(dre) == 0) {
					bullet->SetPosition(GetPosition().x, GetPosition().y, GetPosition().z);

					XMFLOAT3 right = GetRight();
					XMFLOAT3 up = GetUp();
					XMFLOAT3 look = GetLook();

					XMFLOAT4X4 rotationMatrix;
					rotationMatrix._11 = right.x; rotationMatrix._12 = right.y; rotationMatrix._13 = right.z; rotationMatrix._14 = 0.0f;
					rotationMatrix._21 = up.x;    rotationMatrix._22 = up.y;    rotationMatrix._23 = up.z;    rotationMatrix._24 = 0.0f;
					rotationMatrix._31 = look.x;  rotationMatrix._32 = look.y;  rotationMatrix._33 = look.z;  rotationMatrix._34 = 0.0f;
					rotationMatrix._41 = 0.0f;    rotationMatrix._42 = 0.0f;    rotationMatrix._43 = 0.0f;    rotationMatrix._44 = 1.0f;

					bullet->SetRotationTransform(&rotationMatrix);

					SwitchShot();
				}
			}
		}
	}

	UpdateBoundingBox();
	bullet->UpdateBoundingBox();
}

void CTankObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	if (IsExist()) {
		if (!m_bBlowingUp)
		{
			CGameObject::Render(pd3dCommandList, pCamera);
			if (shot)
				bullet->Render(pd3dCommandList, pCamera);
		}
	}
}

void CTankObject::PrepareExplosion()
{
	m_bBlowingUp = true;
	m_fElapsedTimes = 0.0f;

	for (int i = 0; i < EXPLOSION_DEBRISES; i++) {
		XMStoreFloat3(&m_pxmf3SphereVectors[i], RandomUnitVectorOnSphere());
		m_pExplosionObjects->Draw[i] = true;
	}
}

void CTankObject::ReleaseUploadBuffers()
{
	CGameObject::ReleaseUploadBuffers();
	if (bullet) bullet->ReleaseUploadBuffers();
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CHeightMapTerrain::CHeightMapTerrain(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
	* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, LPCTSTR pFileName, int
	nWidth, int nLength, int nBlockWidth, int nBlockLength, XMFLOAT3 xmf3Scale, XMFLOAT4
	xmf4Color) : CGameObject(0)
{
	m_nWidth = nWidth;
	m_nLength = nLength;

	int cxQuadsPerBlock = nBlockWidth - 1;
	int czQuadsPerBlock = nBlockLength - 1;

	m_xmf3Scale = xmf3Scale;
	m_pHeightMapImage = new CHeightMapImage(pFileName, nWidth, nLength, xmf3Scale);
	long cxBlocks = (m_nWidth - 1) / cxQuadsPerBlock;
	long czBlocks = (m_nLength - 1) / czQuadsPerBlock;
	m_nMeshes = cxBlocks * czBlocks;
	m_ppMeshes = new CMesh*[m_nMeshes];

	for (int i = 0; i < m_nMeshes; i++)m_ppMeshes[i] = NULL;
	CHeightMapGridMesh* pHeightMapGridMesh = NULL;
	for (int z = 0, zStart = 0; z < czBlocks; z++)
	{
		for (int x = 0, xStart = 0; x < cxBlocks; x++)
		{
			xStart = x * (nBlockWidth - 1);
			zStart = z * (nBlockLength - 1);
			pHeightMapGridMesh = new CHeightMapGridMesh(pd3dDevice, pd3dCommandList, xStart,
			zStart, nBlockWidth, nBlockLength, xmf3Scale, xmf4Color, m_pHeightMapImage);
			SetMesh(x + (z * cxBlocks), pHeightMapGridMesh);
		}
	}
}
CHeightMapTerrain::~CHeightMapTerrain(void)
{
	if (m_pHeightMapImage) delete m_pHeightMapImage;
}
