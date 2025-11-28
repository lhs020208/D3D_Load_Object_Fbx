//------------------------------------------------------- ----------------------
// File: Object.h
//-----------------------------------------------------------------------------

#pragma once

#include "Mesh.h"
#include "Camera.h"

class CShader;
class CScene;

class CGameObject
{
public:
	CGameObject(int nMeshes = 1);
	virtual ~CGameObject();

public:
	XMFLOAT4X4						m_xmf4x4World;
	XMFLOAT3						m_xmf3Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);

	CMesh** m_ppMeshes = NULL;
	int m_nMeshes = 0;

	CShader							*m_pShader = NULL;

	XMFLOAT3						m_xmf3Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
	BoundingOrientedBox				m_xmOOBB = BoundingOrientedBox();

	void SetMesh(int nIndex, CMesh* pMesh);
	void SetShader(CShader *pShader);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World);
	virtual void ReleaseShaderVariables();

	virtual void Animate(float fTimeElapsed);
	virtual void OnPrepareRender() { }
	virtual void Render(ID3D12GraphicsCommandList* cmdList, CCamera* pCamera, CScene* pScene);
	virtual void ReleaseUploadBuffers();

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	void LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void LookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up);

	virtual void UpdateBoundingBox();

	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);
	void SetRotationTransform(XMFLOAT4X4* pmxf4x4Transform);

	void SetColor(XMFLOAT3 xmf3Color) { m_xmf3Color = xmf3Color; }

	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	virtual void Rotate(float fPitch = 0.0f, float fYaw = 0.0f, float fRoll = 0.0f);
	virtual void Rotate(XMFLOAT3 *pxmf3Axis, float fAngle);

	virtual void SetScale(float x, float y, float z);

	int PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);
	void GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection);

	float FallingSpeed = 0.0f;
	float Height;
	XMFLOAT3 LastUpVector = GetUp();  // 초기화 필요

	ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap = nullptr;
	UINT m_nSrvDescriptorIncrementSize = 0;

	void SetSrvDescriptorInfo(ID3D12DescriptorHeap* heap, UINT inc);
	void PlayAnimation(const std::string& name, bool loop = true, float start = 0.0f);
	void SetNextAnimation(const std::string& clip);
};