//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "GameFramework.h"
#include "AssetManager.h"
#include "CAnimator.h"

extern CGameFramework* g_pFramework;

CScene::CScene(CPlayer* pPlayer)
{
	m_pPlayer = pPlayer;
}

CScene::~CScene()
{
}

void CScene::BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, 
	ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap, UINT m_nSrvDescriptorIncrementSize)
{
	
}

ID3D12RootSignature* CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;

	// 총 root parameter 개수 = 기존 5개 + Texture용 1개 = 6개
	D3D12_ROOT_PARAMETER pd3dRootParameters[6];

	// -------------------------------------------------------------
	// [0] 프레임워크 정보 (Time, Cursor 등)
	// -------------------------------------------------------------
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[0].Constants.Num32BitValues = 4;
	pd3dRootParameters[0].Constants.ShaderRegister = 0;
	pd3dRootParameters[0].Constants.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// -------------------------------------------------------------
	// [1] GameObject World + Color
	// -------------------------------------------------------------
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 19;
	pd3dRootParameters[1].Constants.ShaderRegister = 1;
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// -------------------------------------------------------------
	// [2] Camera matrices
	// -------------------------------------------------------------
	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[2].Constants.Num32BitValues = 35;
	pd3dRootParameters[2].Constants.ShaderRegister = 2;
	pd3dRootParameters[2].Constants.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// -------------------------------------------------------------
	// [3] Light CBV (b3)
	// -------------------------------------------------------------
	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[3].Descriptor.ShaderRegister = 3;
	pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// -------------------------------------------------------------
	// [4] Bone Transform CBV (b4)
	// -------------------------------------------------------------
	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[4].Descriptor.ShaderRegister = 4;
	pd3dRootParameters[4].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// -------------------------------------------------------------
	// [5] Texture DescriptorTable (t0)
	// -------------------------------------------------------------
	CD3DX12_DESCRIPTOR_RANGE texRange;
	texRange.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRV
		1,                               // 1개
		0,                               // baseShaderRegister = t0
		0,                               // register space
		0                                // offset
	);

	pd3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[5].DescriptorTable.pDescriptorRanges = &texRange;
	pd3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// -------------------------------------------------------------
	// Static Sampler (s0)
	// -------------------------------------------------------------
	CD3DX12_STATIC_SAMPLER_DESC samplerDesc(
		0,                                 // register s0
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // Linear filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	// -------------------------------------------------------------
	// Root Signature
	// -------------------------------------------------------------
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = _countof(pd3dRootParameters);
	rootSigDesc.pParameters = pd3dRootParameters;
	rootSigDesc.NumStaticSamplers = 1;
	rootSigDesc.pStaticSamplers = &samplerDesc;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* sigBlob = nullptr;
	ID3DBlob* errBlob = nullptr;

	D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&sigBlob,
		&errBlob
	);

	pd3dDevice->CreateRootSignature(
		0,
		sigBlob->GetBufferPointer(),
		sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&pd3dGraphicsRootSignature)
	);

	if (sigBlob) sigBlob->Release();
	if (errBlob) errBlob->Release();

	return pd3dGraphicsRootSignature;
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

void CScene::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera, ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap)
{
}
void CScene::BuildGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);
}
void CScene::CreateLightConstantBuffer(ID3D12Device* device)
{
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = 256; // 256바이트 정렬
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&m_pLightCB)
	);
}
//탱크 Scene////////////////////////////////////////////////////////////////////////////////////////////////
CTankScene::CTankScene(CPlayer* pPlayer) : CScene(pPlayer) {}

struct LIGHT_CB
{
	XMFLOAT3 LightDirection; float pad1 = 0.0f;
	XMFLOAT3 LightColor;     float pad2 = 0.0f;
};

void CTankScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, 
	ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap, UINT m_nSrvDescriptorIncrementSize)
{
	//m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);
	CSkinnedLightingShader* pShader = new CSkinnedLightingShader();
	pShader->CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature);
	pShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	LIGHT_CB lightData = { m_xmf3LightDirection, 0.0f, m_xmf3LightColor, 0.0f };

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	// 리소스 디스크립터
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Alignment = 0;
	resDesc.Width = 256;
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// 리소스 생성
	HRESULT hr = pd3dDevice->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&m_pLightCB)
	);
	if (FAILED(hr)) OutputDebugString(L"CreateCommittedResource for LightCB failed!\n");

	// 데이터 복사 (한 번만)
	void* pMapped = nullptr;
	m_pLightCB->Map(0, nullptr, &pMapped);
	memcpy(pMapped, &lightData, sizeof(LIGHT_CB));
	m_pLightCB->Unmap(0, nullptr);

	m_pPlayer->SetSrvDescriptorInfo(m_pd3dSrvDescriptorHeap,m_nSrvDescriptorIncrementSize);
	UINT nextSrvIndex = 0;

	// 1) Mesh 로드
	CMesh* mesh = new CMesh(pd3dDevice, pd3dCommandList, "Models/unitychan.fbx", 2);
	//CMesh* pCubeMesh = new CMesh(pd3dDevice, pd3dCommandList, "Models/HumanCharacterDummy_F.fbx", 2);

	// 1-1) Animator 생성 + Mesh에 연결 (스켈레톤 정보 전달 + EnableSkinning)
	CAnimator* pAnimator = new CAnimator();
	mesh->SetAnimator(pAnimator);

	// 1-2) 애니메이션 FBX 로드 → Animator에 클립 등록
	mesh->LoadAnimationFromFBX("Models/unitychan_JUMP00.fbx");

	// 1-3) 사용할 클립 선택 + 루프/재생 설정
	//      - 0번 클립이 unitychan_JUMP00 이라고 가정
	pAnimator->SetClip(0, true); // (clipIndex = 0, loop = true)
	pAnimator->Play();

	// 2) GameObject에 Mesh 장착
	m_pPlayer->SetMesh(0, mesh);
	m_pPlayer->SetSrvDescriptorInfo(m_pd3dSrvDescriptorHeap, m_nSrvDescriptorIncrementSize);

	// 3) Asset 타입 결정
	AssetType assetType = AssetType::UnityChan;

	// 4) SubMesh 자동 텍스처 매핑
	UINT baseSRVIndex = 30; // 예시: 이 모델은 SRV 30~39 사용
	int subIdx = 0;

	for (auto& sm : mesh->m_SubMeshes)
	{
		// 4-1) SubMesh 이름 기반으로 텍스처 파일명 얻기
		std::string texFile = GetTextureFileNameForSubMesh(sm, assetType);

		// 전체 경로 구성
		std::wstring wpath = ToWstring(std::string("Models/Texture/") + texFile);

		// 4-2) 해당 SubMesh에 텍스처 로드 후 SRV 만들기
		mesh->LoadTextureFromFile(
			pd3dDevice, pd3dCommandList, m_pd3dSrvDescriptorHeap,
			baseSRVIndex + subIdx, // 이 SubMesh가 쓸 SRV index
			wpath.c_str(),
			subIdx                  // SubMesh.textureIndex = baseSRVIndex + subIdx;
		);

		subIdx++;
	}

	m_pPlayer->SetPosition(0.0f, 0.0f, 0.0f);
	m_pPlayer->SetCameraOffset(XMFLOAT3(0.0f, -0.0f, -2.0f));

	m_pPlayer -> CreateShaderVariables(pd3dDevice, pd3dCommandList);
	m_pPlayer -> SetShader(pShader);
}

void CTankScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature->Release();
}
void CTankScene::ReleaseUploadBuffers()
{
}
void CTankScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap)
{
	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList); 
	
	ID3D12DescriptorHeap* ppHeaps[] = { m_pd3dSrvDescriptorHeap };
	pd3dCommandList->SetDescriptorHeaps(1, ppHeaps);

	float light[6] = {
		m_xmf3LightDirection.x,
		m_xmf3LightDirection.y,
		m_xmf3LightDirection.z,
		m_xmf3LightColor.x,
		m_xmf3LightColor.y,
		m_xmf3LightColor.z,
	};
	if (m_pLightCB)
		pd3dCommandList->SetGraphicsRootConstantBufferView(3, m_pLightCB->GetGPUVirtualAddress());

	if (m_pPlayer) m_pPlayer->Render(pd3dCommandList, pCamera);
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
	return(pNearestObject);

}
void CTankScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
}

void CTankScene::Animate(float fElapsedTime)
{

	XMFLOAT3 xmf3Position = m_pPlayer->GetPosition();
	m_pPlayer->Animate(fElapsedTime);
}