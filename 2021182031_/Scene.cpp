//-----------------------------------------------------------------------------
// File: CScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "GameFramework.h"
#include "AssetManager.h"
#include "Animator.h"

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

void CTankScene::BuildObjects(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap,
	UINT m_nSrvDescriptorIncrementSize)
{
	//=====================================================================
	// 0) 쉐이더 생성
	//=====================================================================
	CSkinnedLightingShader* pShader = new CSkinnedLightingShader();
	pShader->CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature);
	pShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	//=====================================================================
	// 1) Light CB 생성
	//=====================================================================
	LIGHT_CB lightData = { m_xmf3LightDirection, 0.0f, m_xmf3LightColor, 0.0f };

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = 256;   // CB는 256바이트 정렬
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = S_OK;
	void* pMapped = nullptr;

	hr = pd3dDevice->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE,
		&resDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&m_pLightCB)
	);
	if (FAILED(hr)) OutputDebugString(L"[TankScene] LightCB Create failed!\n");

	m_pLightCB->Map(0, nullptr, &pMapped);
	memcpy(pMapped, &lightData, sizeof(LIGHT_CB));
	m_pLightCB->Unmap(0, nullptr);

	// Player에 SRV heap 정보 전달
	m_pPlayer->SetSrvDescriptorInfo(m_pd3dSrvDescriptorHeap, m_nSrvDescriptorIncrementSize);

	//=====================================================================
	// 2) Mesh
	//=====================================================================
	CMesh* UnitychanMesh = new CMesh(pd3dDevice, pd3dCommandList, "Models/unitychan.bin", 1);
	//CMesh* UnitychanMesh = new CMesh(pd3dDevice, pd3dCommandList, "Models/orcGM.bin", 1);

	int boneCount = UnitychanMesh->GetBoneCount();
	if (boneCount > 0)
		UnitychanMesh->EnableSkinning(boneCount);
	

	//=====================================================================
	// 3) Player Mesh 
	//=====================================================================
	m_pPlayer->SetMesh(0, UnitychanMesh);
	m_pPlayer->SetSrvDescriptorInfo(m_pd3dSrvDescriptorHeap, m_nSrvDescriptorIncrementSize);


	//=====================================================================
	// 4) SubMesh 자동 텍스처 매핑
	//=====================================================================
	AssetType assetType = AssetType::UnityChan;
	//AssetType assetType = AssetType::Orc;
	UINT baseSRVIndex = 30;
	int subIdx = 0;

	for (auto& sm : UnitychanMesh->m_SubMeshes)
	{
		std::string texFile = GetTextureFileNameForSubMesh(sm, assetType);
		std::wstring wpath = ToWstring(std::string("Models/UnitychanTexture/") + texFile);
		//std::wstring wpath = ToWstring(std::string("Models/OrcTexture/") + texFile);

		UnitychanMesh->LoadTextureFromFile(
			pd3dDevice,
			pd3dCommandList,
			m_pd3dSrvDescriptorHeap,
			baseSRVIndex + subIdx,
			wpath.c_str(),
			subIdx
		);

		subIdx++;
	}
	//=====================================================================
	// 5) Player 설정
	//=====================================================================
	m_pPlayer->SetPosition(0.0f, 0.0f, 0.0f);
	m_pPlayer->SetCameraOffset(XMFLOAT3(0.0f, 100.0f, 200.0f));
	m_pPlayer->CreateShaderVariables(pd3dDevice, pd3dCommandList);
	m_pPlayer->SetShader(pShader);

	//=====================================================================
	// 6) DefaultBoneCB 생성 (스키닝 없음 메시용)
	//=====================================================================
	const int MAX_BONES = 256;

	XMFLOAT4X4 identity;
	XMStoreFloat4x4(&identity, XMMatrixIdentity());
	std::vector<XMFLOAT4X4> defaultBones(MAX_BONES, identity);

	UINT cbSize = sizeof(XMFLOAT4X4) * MAX_BONES;
	cbSize = (cbSize + 255) & ~255;  // 256바이트 정렬

	if (m_pDefaultBoneCB)
	{
		m_pDefaultBoneCB->Release();
		m_pDefaultBoneCB = nullptr;
	}

	D3D12_HEAP_PROPERTIES boneHeapProps = {};
	boneHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	boneHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	boneHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	boneHeapProps.CreationNodeMask = 1;
	boneHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC boneResDesc = {};
	boneResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	boneResDesc.Alignment = 0;
	boneResDesc.Width = cbSize;
	boneResDesc.Height = 1;
	boneResDesc.DepthOrArraySize = 1;
	boneResDesc.MipLevels = 1;
	boneResDesc.Format = DXGI_FORMAT_UNKNOWN;
	boneResDesc.SampleDesc.Count = 1;
	boneResDesc.SampleDesc.Quality = 0;
	boneResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	boneResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hrBone = pd3dDevice->CreateCommittedResource(
		&boneHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&boneResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pDefaultBoneCB)
	);

	if (FAILED(hrBone))
	{
		OutputDebugStringA("[TankScene] Failed to create default bone CB.\n");
	}
	else
	{
		void* pBone = nullptr;
		m_pDefaultBoneCB->Map(0, nullptr, &pBone);
		memcpy(pBone, defaultBones.data(), sizeof(XMFLOAT4X4) * MAX_BONES);
		m_pDefaultBoneCB->Unmap(0, nullptr);

		OutputDebugStringA("[TankScene] DefaultBoneCB created.\n");
	}

	//=====================================================================
	// 7) UnityChan 애니메이션(JUMP00) 로드 & Animator에 등록 + 재생
	//=====================================================================
	
	{
		
		AnimationClip jumpClip;

		bool animLoaded = UnitychanMesh->LoadAnimationFromBIN(
			"Models/unitychan_JUMP00.bin", "Jump", jumpClip, 1.0f
		);


		AnimationClip idleClip;
		bool idleLoaded = UnitychanMesh->LoadAnimationFromBIN(
			"Models/unitychan_WAIT00.bin", "Idle", idleClip, 1.0f
			//"Models/orcGA.bin", "Idle", idleClip, 1.0f
		);


		jumpClip.name = "Jump";
		idleClip.name = "Idle";

		CAnimator* pAnimator = UnitychanMesh->EnsureAnimator();
		if (pAnimator)
		{
			pAnimator->AddClip(jumpClip);
			pAnimator->AddClip(idleClip);
		}

	}
	m_pPlayer->PlayAnimation("Idle", true, 0.0f);
	
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

	// b4: 디폴트 Bone CB (스키닝 없는 메시용 기본값)
	if (m_pDefaultBoneCB)
		pd3dCommandList->SetGraphicsRootConstantBufferView(4, m_pDefaultBoneCB->GetGPUVirtualAddress());

	// 이후 객체 렌더
	if (m_pPlayer)
		m_pPlayer->Render(pd3dCommandList, pCamera, this);
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
			m_pPlayer->PlayAnimation("Jump", false, 0.0f);
			m_pPlayer->SetNextAnimation("Idle");
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