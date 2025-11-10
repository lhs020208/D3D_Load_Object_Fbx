#include "stdafx.h"
#include "Player.h"
#include "Camera.h"

CCamera::CCamera()
{
	m_xmf4x4View = Matrix4x4::Identity();
	m_xmf4x4Projection = Matrix4x4::Identity();
	m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
	m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT };
	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_fPitch = 0.0f;
	m_fRoll = 0.0f;
	m_fYaw = 0.0f;
	m_xmf3Offset = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fTimeLag = 0.0f;
	m_xmf3LookAtWorld = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_nMode = 0x00;
	m_pPlayer = NULL;
}

CCamera::CCamera(CCamera *pCamera)
{
	if (pCamera)
	{
		*this = *pCamera;
	}
	else
	{
		m_xmf4x4View = Matrix4x4::Identity();
		m_xmf4x4Projection = Matrix4x4::Identity();
		m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
		m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT };
		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
		m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
		m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
		m_fPitch = 0.0f;
		m_fRoll = 0.0f;
		m_fYaw = 0.0f;
		m_xmf3Offset = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_fTimeLag = 0.0f;
		m_xmf3LookAtWorld = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_nMode = 0x00;
		m_pPlayer = NULL;
	}
}

CCamera::~CCamera()
{
}

void CCamera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ, float fMaxZ)
{
	m_d3dViewport.TopLeftX = float(xTopLeft);
	m_d3dViewport.TopLeftY = float(yTopLeft);
	m_d3dViewport.Width = float(nWidth);
	m_d3dViewport.Height = float(nHeight);
	m_d3dViewport.MinDepth = fMinZ;
	m_d3dViewport.MaxDepth = fMaxZ;
}

void CCamera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
{
	m_d3dScissorRect.left = xLeft;
	m_d3dScissorRect.top = yTop;
	m_d3dScissorRect.right = xRight;
	m_d3dScissorRect.bottom = yBottom;
}

void CCamera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle)
{
	//	m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle), fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
	XMMATRIX xmmtxProjection = XMMatrixPerspectiveFovLH(XMConvertToRadians(fFOVAngle), fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
	XMStoreFloat4x4(&m_xmf4x4Projection, xmmtxProjection);

#ifdef _WITH_DIERECTX_MATH_FRUSTUM
	BoundingFrustum::CreateFromMatrix(m_xmFrustumView, xmmtxProjection);
#endif
}

void CCamera::GenerateOrthographicProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fWidth, float hHeight)
{
	XMMATRIX xmmtxProjection = XMMatrixOrthographicLH(fWidth, hHeight, fNearPlaneDistance, fFarPlaneDistance);
	XMStoreFloat4x4(&m_xmf4x4OrthographicProject, xmmtxProjection);
}

void CCamera::CalculateFrustumPlanes()
{
#ifdef _WITH_DIERECTX_MATH_FRUSTUM
	m_xmFrustumView.Transform(m_xmFrustumWorld, XMMatrixInverse(NULL, XMLoadFloat4x4(&m_xmf4x4View)));
#else
	XMFLOAT4X4 mtxViewProjection = Matrix4x4::Multiply(m_xmf4x4View, m_xmf4x4Projection);

	m_pxmf4FrustumPlanes[0].x = -(mtxViewProjection._14 + mtxViewProjection._11);
	m_pxmf4FrustumPlanes[0].y = -(mtxViewProjection._24 + mtxViewProjection._21);
	m_pxmf4FrustumPlanes[0].z = -(mtxViewProjection._34 + mtxViewProjection._31);
	m_pxmf4FrustumPlanes[0].w = -(mtxViewProjection._44 + mtxViewProjection._41);

	m_pxmf4FrustumPlanes[1].x = -(mtxViewProjection._14 - mtxViewProjection._11);
	m_pxmf4FrustumPlanes[1].y = -(mtxViewProjection._24 - mtxViewProjection._21);
	m_pxmf4FrustumPlanes[1].z = -(mtxViewProjection._34 - mtxViewProjection._31);
	m_pxmf4FrustumPlanes[1].w = -(mtxViewProjection._44 - mtxViewProjection._41);

	m_pxmf4FrustumPlanes[2].x = -(mtxViewProjection._14 - mtxViewProjection._12);
	m_pxmf4FrustumPlanes[2].y = -(mtxViewProjection._24 - mtxViewProjection._22);
	m_pxmf4FrustumPlanes[2].z = -(mtxViewProjection._34 - mtxViewProjection._32);
	m_pxmf4FrustumPlanes[2].w = -(mtxViewProjection._44 - mtxViewProjection._42);

	m_pxmf4FrustumPlanes[3].x = -(mtxViewProjection._14 + mtxViewProjection._12);
	m_pxmf4FrustumPlanes[3].y = -(mtxViewProjection._24 + mtxViewProjection._22);
	m_pxmf4FrustumPlanes[3].z = -(mtxViewProjection._34 + mtxViewProjection._32);
	m_pxmf4FrustumPlanes[3].w = -(mtxViewProjection._44 + mtxViewProjection._42);

	m_pxmf4FrustumPlanes[4].x = -(mtxViewProjection._13);
	m_pxmf4FrustumPlanes[4].y = -(mtxViewProjection._23);
	m_pxmf4FrustumPlanes[4].z = -(mtxViewProjection._33);
	m_pxmf4FrustumPlanes[4].w = -(mtxViewProjection._43);

	m_pxmf4FrustumPlanes[5].x = -(mtxViewProjection._14 - mtxViewProjection._13);
	m_pxmf4FrustumPlanes[5].y = -(mtxViewProjection._24 - mtxViewProjection._23);
	m_pxmf4FrustumPlanes[5].z = -(mtxViewProjection._34 - mtxViewProjection._33);
	m_pxmf4FrustumPlanes[5].w = -(mtxViewProjection._44 - mtxViewProjection._43);

	for (int i = 0; i < 6; i++) m_pxmf4FrustumPlanes[i] = Plane::Normalize(m_pxmf4FrustumPlanes[i]);
#endif
}

bool CCamera::IsInFrustum(BoundingOrientedBox& xmBoundingBox)
{
#ifdef _WITH_DIERECTX_MATH_FRUSTUM
	return(m_xmFrustumWorld.Intersects(xmBoundingBox));
#else
#endif
}

bool CCamera::IsInFrustum(BoundingBox& xmBoundingBox)
{
#ifdef _WITH_DIERECTX_MATH_FRUSTUM
	return(m_xmFrustumWorld.Intersects(xmBoundingBox));
#else
	XMFLOAT3 xmf3NearPoint, xmf3Normal;
	XMFLOAT3 xmf3Minimum = Vector3::Subtract(xmBoundingBox.Center, xmBoundingBox.Extents);
	XMFLOAT3 xmf3Maximum = Vector3::Add(xmBoundingBox.Center, xmBoundingBox.Extents);
	for (int i = 0; i < 6; i++)
	{
		xmf3Normal = XMFLOAT3(m_pxmf4FrustumPlanes[i].x, m_pxmf4FrustumPlanes[i].y, m_pxmf4FrustumPlanes[i].z);
		if (xmf3Normal.x >= 0.0f)
		{
			if (xmf3Normal.y >= 0.0f)
			{
				if (xmf3Normal.z >= 0.0f)
				{
					xmf3NearPoint = XMFLOAT3(xmf3Minimum.x, xmf3Minimum.y, xmf3Minimum.z);
				}
				else
				{
					xmf3NearPoint = XMFLOAT3(xmf3Minimum.x, xmf3Minimum.y, xmf3Maximum.z);
				}
			}
			else
			{
				if (xmf3Normal.z >= 0.0f)
				{
					xmf3NearPoint = XMFLOAT3(xmf3Minimum.x, xmf3Maximum.y, xmf3Minimum.z);
				}
				else
				{
					xmf3NearPoint = XMFLOAT3(xmf3Minimum.x, xmf3Maximum.y, xmf3Maximum.z);
				}
			}
		}
		else
		{
			if (xmf3Normal.y >= 0.0f)
			{
				if (xmf3Normal.z >= 0.0f)
				{
					xmf3NearPoint = XMFLOAT3(xmf3Maximum.x, xmf3Minimum.y, xmf3Minimum.z);
				}
				else
				{
					xmf3NearPoint = XMFLOAT3(xmf3Maximum.x, xmf3Minimum.y, xmf3Maximum.z);
				}
			}
			else
			{
				if (xmf3Normal.z >= 0.0f)
				{
					xmf3NearPoint = XMFLOAT3(xmf3Maximum.x, xmf3Maximum.y, xmf3Minimum.z);
				}
				else
				{
					xmf3NearPoint = XMFLOAT3(xmf3Maximum.x, xmf3Maximum.y, xmf3Maximum.z);
				}
			}
		}
		if ((Vector3::DotProduct(xmf3Normal, xmf3NearPoint) + m_pxmf4FrustumPlanes[i].w) > 0.0f) return(false);
	}

	return(true);
#endif
}

void CCamera::GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up)
{
	m_xmf3Position = xmf3Position;
	m_xmf3LookAtWorld = xmf3LookAt;
	m_xmf3Up = xmf3Up;

	GenerateViewMatrix();
}

void CCamera::GenerateViewMatrix()
{
	if (Vector3::Equal(m_xmf3Position, m_xmf3LookAtWorld, 0.0001f))
		m_xmf3LookAtWorld.z += 0.01f;
	m_xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, m_xmf3LookAtWorld, m_xmf3Up);
}

void CCamera::RegenerateViewMatrix()
{
	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);

	m_xmf4x4View._11 = m_xmf3Right.x; m_xmf4x4View._12 = m_xmf3Up.x; m_xmf4x4View._13 = m_xmf3Look.x;
	m_xmf4x4View._21 = m_xmf3Right.y; m_xmf4x4View._22 = m_xmf3Up.y; m_xmf4x4View._23 = m_xmf3Look.y;
	m_xmf4x4View._31 = m_xmf3Right.z; m_xmf4x4View._32 = m_xmf3Up.z; m_xmf4x4View._33 = m_xmf3Look.z;
	m_xmf4x4View._41 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Right);
	m_xmf4x4View._42 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Up);
	m_xmf4x4View._43 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Look);

	m_xmf4x4InverseView._11 = m_xmf3Right.x; m_xmf4x4InverseView._12 = m_xmf3Right.y; m_xmf4x4InverseView._13 = m_xmf3Right.z;
	m_xmf4x4InverseView._21 = m_xmf3Up.x; m_xmf4x4InverseView._22 = m_xmf3Up.y; m_xmf4x4InverseView._23 = m_xmf3Up.z;
	m_xmf4x4InverseView._31 = m_xmf3Look.x; m_xmf4x4InverseView._32 = m_xmf3Look.y; m_xmf4x4InverseView._33 = m_xmf3Look.z;
	m_xmf4x4InverseView._41 = m_xmf3Position.x; m_xmf4x4InverseView._42 = m_xmf3Position.y; m_xmf4x4InverseView._43 = m_xmf3Position.z;

	m_xmFrustumView.Transform(m_xmFrustumWorld, XMLoadFloat4x4(&m_xmf4x4InverseView));
}

void CCamera::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(2, 16, &xmf4x4View, 0);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(2, 16, &xmf4x4Projection, 16);

	pd3dCommandList->SetGraphicsRoot32BitConstants(2, 3, &m_xmf3Position, 32);
}

void CCamera::ReleaseShaderVariables()
{
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList *pd3dCommandList)
{
	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CThirdPersonCamera

CThirdPersonCamera::CThirdPersonCamera(CCamera *pCamera) : CCamera(pCamera)
{
	m_nMode = THIRD_PERSON_CAMERA;
	if (pCamera)
	{
		if (pCamera->GetMode() == THIRD_PERSON_CAMERA)
		{
			m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
			m_xmf3Right.y = 0.0f;
			m_xmf3Look.y = 0.0f;
			m_xmf3Right = Vector3::Normalize(m_xmf3Right);
			m_xmf3Look = Vector3::Normalize(m_xmf3Look);
		}
	}
}

void CThirdPersonCamera::Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed)
{
	if (m_pPlayer)
	{
		if (m_pPlayer->overview == false) {
			XMVECTOR up = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->m_xmf3Up));
			XMVECTOR look = XMVector3Normalize(XMLoadFloat3(&m_pPlayer->m_xmf3Look));

			// 2. Right = Up x Look
			XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, look));

			// 3. Look 재정렬 = Right x Up
			look = XMVector3Normalize(XMVector3Cross(right, up)); // 정직교 보정

			// 4. 회전 행렬 구성
			XMMATRIX mtxRotate;
			mtxRotate.r[0] = right;
			mtxRotate.r[1] = up;
			mtxRotate.r[2] = look;
			mtxRotate.r[3] = XMVectorSet(0, 0, 0, 1);

			// 5. 카메라 위치 계산
			XMFLOAT3 xmf3Offset = Vector3::TransformCoord(m_pPlayer->m_xmf3CameraOffset, mtxRotate);
			XMFLOAT3 xmf3Position = Vector3::Add(m_pPlayer->m_xmf3Position, xmf3Offset);

			// 6. 위치 적용
			m_xmf3Position = xmf3Position;

			// 7. LookAt 적용 (위치를 바라보게)
			//SetLookAt(m_pPlayer->m_xmf3Position, XMFLOAT3(0.0f, 1.0f, 0.0f));

			// 8. 뷰 행렬 갱신
			//GenerateViewMatrix();
		}
		else
		{
			m_xmf3Position = XMFLOAT3(-30.0f, 10.0f, 30.0f);

			XMFLOAT3 target = XMFLOAT3(0.0f, 0.0f, 0.0f);

			if (Vector3::Equal(m_xmf3Position, target, 0.0001f)) target.z += 0.01f;
			SetLookAt(target);

			GenerateViewMatrix();
		}
	}
}

void CThirdPersonCamera::SetLookAt(XMFLOAT3& xmf3LookAt)
{
	
	XMVECTOR eye = XMLoadFloat3(&m_xmf3Position);
	XMVECTOR at = XMLoadFloat3(&xmf3LookAt);
	XMVECTOR dir = XMVectorSubtract(at, eye);

	if (XMVector3Equal(dir, XMVectorZero()))
	{
		// 보정: eye와 at가 같을 때 작은 z 이동
		xmf3LookAt.z += 0.01f;
	}

	XMFLOAT4X4 mtxLookAt = Matrix4x4::LookAtLH(m_xmf3Position, xmf3LookAt, m_pPlayer->GetUpVector());
	m_xmf3Right = Vector3::Normalize(XMFLOAT3(mtxLookAt._11, mtxLookAt._21, mtxLookAt._31));
	m_xmf3Up = Vector3::Normalize(XMFLOAT3(mtxLookAt._12, mtxLookAt._22, mtxLookAt._32));
	m_xmf3Look = Vector3::Normalize(XMFLOAT3(mtxLookAt._13, mtxLookAt._23, mtxLookAt._33));
}

void CThirdPersonCamera::SetLookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
{
	XMVECTOR eye = XMLoadFloat3(&m_xmf3Position);
	XMFLOAT3 xmf3LookAtCopy = xmf3LookAt;
	XMVECTOR at = XMLoadFloat3(&xmf3LookAtCopy);
	XMVECTOR dir = XMVectorSubtract(at, eye);

	if (XMVector3Equal(dir, XMVectorZero()))
	{
		// 보정은 복사본에 적용
		xmf3LookAtCopy.z += 0.01f;
		at = XMLoadFloat3(&xmf3LookAtCopy);
		dir = XMVectorSubtract(at, eye);
	}

	XMFLOAT4X4 viewMatrix = Matrix4x4::LookAtLH(m_xmf3Position, xmf3LookAtCopy, xmf3Up);
	m_xmf3Right = Vector3::Normalize(XMFLOAT3(viewMatrix._11, viewMatrix._21, viewMatrix._31));
	m_xmf3Up = Vector3::Normalize(XMFLOAT3(viewMatrix._12, viewMatrix._22, viewMatrix._32));
	m_xmf3Look = Vector3::Normalize(XMFLOAT3(viewMatrix._13, viewMatrix._23, viewMatrix._33));
}

void CCamera::SetLookAt(XMFLOAT3& xmf3Position, XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
{
	m_xmf3Position = xmf3Position;
	XMFLOAT3 lookAtCopy = xmf3LookAt;

	if (Vector3::Equal(m_xmf3Position, lookAtCopy, 0.0001f))
		lookAtCopy.z += 0.01f;
	
	m_xmf3Position = xmf3Position;
	m_xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, lookAtCopy, xmf3Up);
	m_xmf3Right = Vector3::Normalize(XMFLOAT3(m_xmf4x4View._11, m_xmf4x4View._21, m_xmf4x4View._31));
	m_xmf3Up = Vector3::Normalize(XMFLOAT3(m_xmf4x4View._12, m_xmf4x4View._22, m_xmf4x4View._32));
	m_xmf3Look = Vector3::Normalize(XMFLOAT3(m_xmf4x4View._13, m_xmf4x4View._23, m_xmf4x4View._33));
}
