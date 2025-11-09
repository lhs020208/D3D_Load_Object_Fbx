//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "GameFramework.h"
extern CGameFramework* g_pFramework;

CScene::CScene(CPlayer* pPlayer)
{
	m_pPlayer = pPlayer;
}

CScene::~CScene()
{
}

void CScene::BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
	
}

ID3D12RootSignature *CScene::CreateGraphicsRootSignature(ID3D12Device *pd3dDevice)
{
	ID3D12RootSignature *pd3dGraphicsRootSignature = NULL;

	D3D12_ROOT_PARAMETER pd3dRootParameters[4];
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[0].Constants.Num32BitValues = 4; //Time, ElapsedTime, xCursor, yCursor
	pd3dRootParameters[0].Constants.ShaderRegister = 0; //Time
	pd3dRootParameters[0].Constants.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 19; //16 + 3
	pd3dRootParameters[1].Constants.ShaderRegister = 1; //World
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[2].Constants.Num32BitValues = 35; //16 + 16 + 3
	pd3dRootParameters[2].Constants.ShaderRegister = 2; //Camera
	pd3dRootParameters[2].Constants.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[3].Constants.Num32BitValues = 6; // float 6개 + 패딩 2개
	pd3dRootParameters[3].Constants.ShaderRegister = 3; // b3 
	pd3dRootParameters[3].Constants.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = 4;
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 0;
	d3dRootSignatureDesc.pStaticSamplers = NULL;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ID3DBlob *pd3dSignatureBlob = NULL;
	ID3DBlob *pd3dErrorBlob = NULL;
	D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
	pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void **)&pd3dGraphicsRootSignature);
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
	if (pd3dErrorBlob) pd3dErrorBlob->Release();

	return(pd3dGraphicsRootSignature);
}

void CScene::ReleaseObjects()
{
	
}

void CScene::ReleaseUploadBuffers()
{

}

void CScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
}
void CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
}

bool CScene::ProcessInput()
{
	return(false);
}

void CScene::Animate(float fTimeElapsed)
{
}

void CScene::PrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature);
}

void CScene::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
}
void CScene::BuildGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);
}

//탱크 Scene////////////////////////////////////////////////////////////////////////////////////////////////
CTankScene::CTankScene(CPlayer* pPlayer) : CScene(pPlayer) {}
void CTankScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);
	CLightingShader* pShader = new CLightingShader();
	pShader->CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature);
	pShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	using namespace std;
	default_random_engine dre{ random_device{}() };
	uniform_real_distribution<float> uid{ 0.0f,1.0f };

	uniform_real_distribution<float> uid_x{ 0,100.0f };
	uniform_real_distribution<float> uid_z{ 0,100.0f };
	uniform_real_distribution<float> uid_rot{ 0,360.0f };

	for (int i = 0; i < m_nTanks; i++)
	{
		float red = uid(dre);
		float green = uid(dre);
		float blue = uid(dre);

		m_pTank[i] = nullptr;
		m_pTank[i] = new CTankObject();
		CMesh* pTankMesh = new CMesh(pd3dDevice, pd3dCommandList, "Models/Tank.obj");
		m_pTank[i]->SetMesh(0,pTankMesh);
		m_pTank[i]->SetShader(pShader);
		m_pTank[i]->SetColor(XMFLOAT3(red, green, blue));
		m_pTank[i]->SetPosition(uid_x(dre) - 50.0f, 0.0f, uid_z(dre) - 50.0f);
		m_pTank[i]->Rotate(0.0f, uid_rot(dre), 0.0f);
		m_pTank[i]->UpdateBoundingBox();
	}
}

void CTankScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature->Release();
	for (int i = 0; i < m_nTanks; i++) {
		if (m_pTank[i])delete m_pTank[i];
	}
}
void CTankScene::ReleaseUploadBuffers()
{
	for (int i = 0; i < m_nTanks; i++) {
		if (m_pTank[i]) m_pTank[i]->ReleaseUploadBuffers();
	}
}
void CTankScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);
	float light[6] = {
		m_xmf3LightDirection.x,
		m_xmf3LightDirection.y,
		m_xmf3LightDirection.z,
		m_xmf3LightColor.x,
		m_xmf3LightColor.y,
		m_xmf3LightColor.z,
	};
	pd3dCommandList->SetGraphicsRoot32BitConstants(3, 6, light, 0);

	if (m_pPlayer) m_pPlayer->Render(pd3dCommandList, pCamera);

	for (int i = 0; i < m_nTanks; i++) {
		m_pTank[i]->Render(pd3dCommandList, pCamera);
	}
}
void CTankScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	extern CGameFramework* g_pFramework;
	switch (nMessageID)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 'W':
			if (m_pPlayer->move_z < 1)m_pPlayer->move_z += 1;
			break;
		case 'S':
			if (m_pPlayer->move_z > -1)m_pPlayer->move_z -= 1;
			break;
		case 'A':
			if (m_pPlayer->move_x > -1)m_pPlayer->move_x -= 1;
			break;
		case 'D':
			if (m_pPlayer->move_x < 1)m_pPlayer->move_x += 1;
			break;
		default:
			break;
		}
		break;
	case WM_KEYUP:
		switch (wParam)
		{
		case 'W':
			if (m_pPlayer->move_z > -1)m_pPlayer->move_z -= 1;
			break;
		case 'S':
			if (m_pPlayer->move_z < 1)m_pPlayer->move_z += 1;
			break;
		case 'A':
			if (m_pPlayer->move_x < 1)m_pPlayer->move_x += 1;
			break;
		case 'D':
			if (m_pPlayer->move_x > -1)m_pPlayer->move_x -= 1;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}
CGameObject* CTankScene::PickObjectPointedByCursor(int xClient, int yClient, CCamera* pCamera)
{

	XMFLOAT3 xmf3PickPosition;
	xmf3PickPosition.x = (((2.0f * xClient) / (float)pCamera->m_d3dViewport.Width) - 1) / pCamera->m_xmf4x4Projection._11;
	xmf3PickPosition.y = -(((2.0f * yClient) / (float)pCamera->m_d3dViewport.Height) - 1) / pCamera->m_xmf4x4Projection._22;
	xmf3PickPosition.z = 1.0f;

	XMVECTOR xmvPickPosition = XMLoadFloat3(&xmf3PickPosition);
	XMMATRIX xmmtxView = XMLoadFloat4x4(&pCamera->m_xmf4x4View);

	float fNearestHitDistance = FLT_MAX;
	CGameObject* pNearestObject = NULL;
	for (int i = 0; i < m_nTanks; i++) {
		if (m_pTank[i])
		{
			int hit = m_pTank[i]->PickObjectByRayIntersection(xmvPickPosition, xmmtxView, &fNearestHitDistance);
			if (hit > 0)
			{
				pNearestObject = m_pTank[i];
			}
		}
	}
	return(pNearestObject);

}
void CTankScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
}

void CTankScene::Animate(float fElapsedTime)
{
	for (int i = 0; i < m_nTanks; i++) {
		if (m_pTank[i]) {

			m_pTank[i]->Animate(fElapsedTime);
			XMFLOAT3 xmf3Position = m_pTank[i]->GetPosition();
		}
	}

	XMFLOAT3 xmf3Position = m_pPlayer->GetPosition();
	m_pPlayer->Animate(fElapsedTime);
}