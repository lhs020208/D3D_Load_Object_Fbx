//------------------------------------------------------- ----------------------
// File: Object.h
//-----------------------------------------------------------------------------

#pragma once

#include "Mesh.h"
#include "Camera.h"

class CShader;

class CGameObject
{
public:
	CGameObject(int nMeshes = 1);
	virtual ~CGameObject();

public:
	XMFLOAT4X4						m_xmf4x4World;
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
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera = NULL);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, XMFLOAT4X4* pxmf4x4World);
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

	int PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);
	void GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection);

	float FallingSpeed = 0.0f;
	float Height;
	XMFLOAT3 LastUpVector = GetUp();  // 초기화 필요
};

class CCubeObject : public CGameObject
{
public:

	virtual void Animate(float fElapsedTime) override;
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;

};

class CExplosionObject : public CGameObject
{
public:
	bool Draw[EXPLOSION_DEBRISES];
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;

	XMFLOAT4X4 m_pxmf4x4Transforms[EXPLOSION_DEBRISES];
	XMFLOAT3 m_pxmf3SphereVectors[EXPLOSION_DEBRISES];
};

class CTankObject : public CGameObject
{
public:

	virtual void Animate(float fElapsedTime) override;
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;
	virtual void ReleaseUploadBuffers() override;
	void PrepareExplosion();
	bool IsBlowingUp() { return m_bBlowingUp; }
	bool IsExist() { return is_exist; }
	void SetExist(bool exist) { is_exist = exist; }

	void SwitchShot() { shot = !shot; bullet_timer = 0; }
	bool IsShot() { return shot; }


	XMFLOAT4X4 m_pxmf4x4Transforms[EXPLOSION_DEBRISES];
	XMFLOAT3 m_pxmf3SphereVectors[EXPLOSION_DEBRISES];

	CGameObject* bullet;
	CExplosionObject* m_pExplosionObjects;
private:
	bool is_exist = true;
	bool m_bBlowingUp = false;
	bool m_bPrevBlowingUp = false;
	float m_fElapsedTimes = 0.0f;
	float m_fDuration = 2.0f;
	float m_fExplosionSpeed = 10.0f;
	float m_fExplosionRotation = 360.0f;
	int timer = 0;
	int bullet_timer = 0;
	bool shot = false;
};

class CHeightMapTerrain : public CGameObject
{
public:
	CHeightMapTerrain(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
		* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, LPCTSTR pFileName, int
		nWidth, int nLength, int nBlockWidth, int nBlockLength, XMFLOAT3 xmf3Scale, XMFLOAT4
		xmf4Color);
	virtual ~CHeightMapTerrain();
private:
	CHeightMapImage* m_pHeightMapImage;
	int m_nWidth;
	int m_nLength;
	XMFLOAT3 m_xmf3Scale;
public:
	float GetHeight(float x, float z) {
		return(m_pHeightMapImage->GetHeight(x /
			m_xmf3Scale.x, z / m_xmf3Scale.z) * m_xmf3Scale.y);
	}
	float GetHeight(XMFLOAT3 xmf3Position)
	{
		XMFLOAT3 terrainWorldPos = GetPosition();
		float local_x = xmf3Position.x - terrainWorldPos.x;
		float local_z = xmf3Position.z - terrainWorldPos.z;
		return (GetHeight(local_x, local_z) + terrainWorldPos.y);
	}
	XMFLOAT3 GetNormal(float x, float z) {
		return(m_pHeightMapImage->GetHeightMapNormal(int(x / m_xmf3Scale.x), int(z /
			m_xmf3Scale.z)));
	}
	XMFLOAT3 GetNormal(XMFLOAT3 xmf3Position)
	{
		XMFLOAT3 terrainWorldPos = GetPosition();
		float local_x = xmf3Position.x - terrainWorldPos.x;
		float local_z = xmf3Position.z - terrainWorldPos.z;
		return GetNormal(local_x, local_z);
	}
	int GetHeightMapWidth() { return(m_pHeightMapImage->GetHeightMapWidth()); }
	int GetHeightMapLength() { return(m_pHeightMapImage->GetHeightMapLength()); }
	XMFLOAT3 GetScale() { return(m_xmf3Scale); }
	float GetWidth() { return(m_nWidth * m_xmf3Scale.x); }
	float GetLength() { return(m_nLength * m_xmf3Scale.z); }
};