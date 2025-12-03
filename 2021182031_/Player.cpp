//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "Animator.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayer

CPlayer::CPlayer()
{
	m_pCamera = NULL;

	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fMaxVelocityXZ = 0.0f;
	m_fMaxVelocityY = 0.0f;
	m_fFriction = 0.0f;

	m_fPitch = 0.0f;
	m_fRoll = 0.0f;
	m_fYaw = 0.0f;

	m_pPlayerUpdatedContext = NULL;
	m_pCameraUpdatedContext = NULL;
}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

	if (m_pCamera) delete m_pCamera;
}

void CPlayer::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	if (m_pCamera) m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CGameObject::CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	if (m_pCamera) m_pCamera->UpdateShaderVariables(pd3dCommandList);

	CGameObject::UpdateShaderVariables(pd3dCommandList);
}

void CPlayer::ReleaseShaderVariables()
{
	if (m_pCamera) m_pCamera->ReleaseShaderVariables();

	CGameObject::ReleaseShaderVariables();
}

void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	if (dwDirection)
	{
		XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
		if (dwDirection & DIR_FORWARD) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, fDistance);
		if (dwDirection & DIR_BACKWARD) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, -fDistance);
		if (dwDirection & DIR_RIGHT) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, fDistance);
		if (dwDirection & DIR_LEFT) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, -fDistance);
		if (dwDirection & DIR_UP) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, fDistance);
		if (dwDirection & DIR_DOWN) xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, -fDistance);

		Move(xmf3Shift, bUpdateVelocity);
	}
}

void CPlayer::Move(const XMFLOAT3& xmf3Shift, bool bUpdateVelocity)
{
	if (bUpdateVelocity)
	{
		m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, xmf3Shift);
	}
	else
	{
		m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Shift);
		m_pCamera->Move(xmf3Shift);
	}
}

void CPlayer::Rotate(float x, float y, float z)
{
	DWORD nCurrentCameraMode = m_pCamera->GetMode();
	if (nCurrentCameraMode == THIRD_PERSON_CAMERA)
	{
		if (x != 0.0f)
		{
			m_fPitch += x;
			if (m_fPitch > +89.0f) { x -= (m_fPitch - 89.0f); m_fPitch = +89.0f; }
			if (m_fPitch < -89.0f) { x -= (m_fPitch + 89.0f); m_fPitch = -89.0f; }
		}
		if (y != 0.0f)
		{
			m_fYaw += y;
			if (m_fYaw > 360.0f) m_fYaw -= 360.0f;
			if (m_fYaw < 0.0f) m_fYaw += 360.0f;
		}
		if (z != 0.0f)
		{
			m_fRoll += z;
			if (m_fRoll > +20.0f) { z -= (m_fRoll - 20.0f); m_fRoll = +20.0f; }
			if (m_fRoll < -20.0f) { z -= (m_fRoll + 20.0f); m_fRoll = -20.0f; }
		}
		m_pCamera->Rotate(x, y, z);
		if (y != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Up), XMConvertToRadians(y));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
	}

	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);
}

void CPlayer::Update(float fTimeElapsed)
{
	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, Vector3::ScalarProduct(m_xmf3Gravity, fTimeElapsed, false));
	float fLength = sqrtf(m_xmf3Velocity.x * m_xmf3Velocity.x + m_xmf3Velocity.z * m_xmf3Velocity.z);
	float fMaxVelocityXZ = m_fMaxVelocityXZ * fTimeElapsed;
	if (fLength > m_fMaxVelocityXZ)
	{
		m_xmf3Velocity.x *= (fMaxVelocityXZ / fLength);
		m_xmf3Velocity.z *= (fMaxVelocityXZ / fLength);
	}
	float fMaxVelocityY = m_fMaxVelocityY * fTimeElapsed;
	fLength = sqrtf(m_xmf3Velocity.y * m_xmf3Velocity.y);
	if (fLength > m_fMaxVelocityY) m_xmf3Velocity.y *= (fMaxVelocityY / fLength);

	Move(m_xmf3Velocity, false);

	if (m_pPlayerUpdatedContext) OnPlayerUpdateCallback(fTimeElapsed);

	
	DWORD nCurrentCameraMode = m_pCamera->GetMode();
	//if (nCurrentCameraMode == THIRD_PERSON_CAMERA) m_pCamera->Update(m_xmf3Position, fTimeElapsed);
	if (m_pCameraUpdatedContext) OnCameraUpdateCallback(fTimeElapsed);
	//if (nCurrentCameraMode == THIRD_PERSON_CAMERA) m_pCamera->SetLookAt(m_xmf3Position);
	m_pCamera->RegenerateViewMatrix();

	fLength = Vector3::Length(m_xmf3Velocity);
	float fDeceleration = (m_fFriction * fTimeElapsed);
	if (fDeceleration > fLength) fDeceleration = fLength;
	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, Vector3::ScalarProduct(m_xmf3Velocity, -fDeceleration, true));

	wchar_t buffer[128];
	swprintf_s(buffer, L"[PLAYER POS] x: %.3f, y: %.3f, z: %.3f\n",
		m_xmf3Position.x, m_xmf3Position.y, m_xmf3Position.z);
	//OutputDebugString(buffer);
	
}

CCamera *CPlayer::OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode)
{
	CCamera *pNewCamera = NULL;
	switch (nNewCameraMode)
	{
		case THIRD_PERSON_CAMERA:
			pNewCamera = new CThirdPersonCamera(m_pCamera);
			break;
	}

	if (pNewCamera)
	{
		pNewCamera->SetPlayer(this);
	}

	if (m_pCamera) delete m_pCamera;

	return(pNewCamera);
}

void CPlayer::OnPrepareRender()
{
	m_xmf4x4World._11 = m_xmf3Right.x; m_xmf4x4World._12 = m_xmf3Right.y; m_xmf4x4World._13 = m_xmf3Right.z;
	m_xmf4x4World._21 = m_xmf3Up.x; m_xmf4x4World._22 = m_xmf3Up.y; m_xmf4x4World._23 = m_xmf3Up.z;
	m_xmf4x4World._31 = m_xmf3Look.x; m_xmf4x4World._32 = m_xmf3Look.y; m_xmf4x4World._33 = m_xmf3Look.z;
	m_xmf4x4World._41 = m_xmf3Position.x; m_xmf4x4World._42 = m_xmf3Position.y; m_xmf4x4World._43 = m_xmf3Position.z;
}

void CPlayer::SetPosition(float x, float y, float z)
{
	m_xmf3Position = XMFLOAT3(x, y, z);

	CGameObject::SetPosition(x, y, z);
}

void CPlayer::SetCameraOffset(XMFLOAT3& xmf3CameraOffset)
{
	m_xmf3CameraOffset = xmf3CameraOffset;

	// 카메라 실제 위치
	XMFLOAT3 camPos = Vector3::Add(m_xmf3Position, m_xmf3CameraOffset);

	// 위를 더 보게 하고 싶으면 Y만 살짝 올리면 됨
	XMFLOAT3 target = m_xmf3Position;
	target.y += 100.0f;   // ← 여기가 lookAt 조정 부분

	m_pCamera->SetLookAt(camPos, target, m_xmf3Up);
	m_pCamera->GenerateViewMatrix();
}

void CPlayer::Render(ID3D12GraphicsCommandList* cmdList, CCamera* pCamera, CScene* pScene)
{
	CGameObject::Render(cmdList, pCamera, pScene);
}
void CPlayer::reset()
{
	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_xmf3CameraOffset = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_fFriction = 125.0f;

	m_fPitch = 0.0f;
	m_fYaw = 0.0f;
	m_fRoll = 0.0f;
}

void CPlayer::SetDefaultAnimation(const std::string& clipName,
	float playbackSpeed,
	bool loop,
	bool autoPlay)
{
	m_strDefaultAnimClip = clipName;
	m_fAnimPlaybackSpeed = playbackSpeed;
	m_bAnimLoop = loop;
	m_bAnimAutoPlay = autoPlay;
	m_bAnimInitialized = false; // 다시 초기화해서 다음 Animate에서 Play
}

void CPlayer::Animate(float fElapsedTime)
{
	// 1) 아직 기본 클립을 재생한 적이 없고, 자동 재생이 켜져 있으며, 이름이 설정돼 있으면
	if (!m_bAnimInitialized && m_bAnimAutoPlay && !m_strDefaultAnimClip.empty())
	{
		for (int i = 0; i < m_nMeshes; ++i)
		{
			CMesh* pMesh = m_ppMeshes[i];
			if (!pMesh || !pMesh->IsSkinnedMesh())
				continue;

			CAnimator* pAnimator = pMesh->EnsureAnimator();
			if (!pAnimator) continue;

			// 클립이 실제로 존재하는 경우에만 재생
			if (pAnimator->HasClip(m_strDefaultAnimClip))
			{
				// (loop, startTime=0.0f)
				pAnimator->Play(m_strDefaultAnimClip, m_bAnimLoop, 0.0f);
			}
		}

		m_bAnimInitialized = true;
	}

	// 2) 재생 속도를 반영한 dt로 본 애니메이션 업데이트
	float animDt = fElapsedTime * m_fAnimPlaybackSpeed;

	// CGameObject::Animate 안에서
	//  - 각 Mesh의 Animator->Update(animDt)
	//  - 최종 본행렬 계산 + CMesh::UpdateBoneTransformsOnGPU(...)
	// 를 수행한다고 가정.
	CGameObject::Animate(animDt);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPersonPlayer::CPersonPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	m_pCamera = ChangeCamera(THIRD_PERSON_CAMERA, 0.0f);

	SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
	SetColor(XMFLOAT3(0.5f, 0.5f, 0.5f));

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	CSkinnedLightingShader* pShader = new CSkinnedLightingShader();
	pShader->CreateShader(pd3dDevice, pd3dGraphicsRootSignature);
	pShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	SetShader(pShader);
}
void CPersonPlayer::OnPrepareRender()
{
	CPlayer::OnPrepareRender();
	//m_xmf4x4World = Matrix4x4::Multiply(XMMatrixRotationRollPitchYaw(XMConvertToRadians(90.0f), 0.0f, 0.0f), m_xmf4x4World);
}

void CPersonPlayer::Animate(float fElapsedTime)
{
	// 1) 플레이어의 본 애니메이션 처리 (클립 재생 + 본 행렬 계산/업로드)
	CPlayer::Animate(fElapsedTime);

	// 2) 기존 이동 로직
	XMFLOAT3 look = GetLook();
	XMFLOAT3 right = GetRight();
	XMFLOAT3 moveVec = { 0.0f, 0.0f, 0.0f };

	float speed = fElapsedTime * 0.5f * 1000;

	moveVec.x += right.x * move_x * speed;
	moveVec.z += right.z * move_x * speed;

	moveVec.x += look.x * move_z * speed;
	moveVec.z += look.z * move_z * speed;

	XMFLOAT3 now_pos = GetPosition();
	SetPosition(now_pos.x + moveVec.x, now_pos.y, now_pos.z + moveVec.z);

	CPersonPlayer::OnPrepareRender();
	UpdateBoundingBox();
}

void CPersonPlayer::Render(ID3D12GraphicsCommandList* cmdList, CCamera* pCamera, CScene* pScene)
{
	CPlayer::Render(cmdList, pCamera, pScene);
}


void CPersonPlayer::Rotate(float fPitch, float fYaw, float fRoll)
{
	CPlayer::Rotate(fPitch, fYaw, fRoll);
	XMFLOAT3 right = GetRight();
	XMFLOAT3 up = GetUp();
	XMFLOAT3 look = GetLook();

	XMFLOAT4X4 rotationMatrix;
	rotationMatrix._11 = right.x; rotationMatrix._12 = right.y; rotationMatrix._13 = right.z; rotationMatrix._14 = 0.0f;
	rotationMatrix._21 = up.x;    rotationMatrix._22 = up.y;    rotationMatrix._23 = up.z;    rotationMatrix._24 = 0.0f;
	rotationMatrix._31 = look.x;  rotationMatrix._32 = look.y;  rotationMatrix._33 = look.z;  rotationMatrix._34 = 0.0f;
	rotationMatrix._41 = 0.0f;    rotationMatrix._42 = 0.0f;    rotationMatrix._43 = 0.0f;    rotationMatrix._44 = 1.0f;

	UpdateBoundingBox();
}

CCamera* CPersonPlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
{
	DWORD nCurrentCameraMode = (m_pCamera) ? m_pCamera->GetMode() : 0x00;
	if (nCurrentCameraMode == nNewCameraMode) return(m_pCamera);
	switch (nNewCameraMode)
	{
	case THIRD_PERSON_CAMERA:
		SetFriction(200.0f);
		SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetMaxVelocityXZ(125.0f);
		SetMaxVelocityY(400.0f);
		m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, nCurrentCameraMode);
		m_pCamera->SetTimeLag(0.25f);
		m_pCamera->SetOffset(XMFLOAT3(0.0f, 5.0f, -13.0f));
		m_pCamera->GenerateProjectionMatrix(1.0f, 1000.0f, ASPECT_RATIO, 60.0f);
		m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
		m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
		break;
	default:
		break;
	}

	m_pCamera->SetPosition(Vector3::Add(m_xmf3Position, m_pCamera->GetOffset()));
	Update(fTimeElapsed);

	return(m_pCamera);
}

