//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Mesh.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////

CPolygon::CPolygon(int nVertices)
{
	m_nVertices = nVertices;
	m_pVertices = new CVertex[nVertices];
}

CPolygon::~CPolygon()
{
	if (m_pVertices) delete[] m_pVertices;
}

void CPolygon::SetVertex(int nIndex, CVertex& vertex)
{
	if ((0 <= nIndex) && (nIndex < m_nVertices) && m_pVertices)
	{
		m_pVertices[nIndex] = vertex;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

CMesh::CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName, int FileType)
{
	if (pstrFileName) {
		if (FileType == 1) LoadMeshFromOBJ(pd3dDevice, pd3dCommandList, pstrFileName);
		if (FileType == 2) LoadMeshFromFBX(pd3dDevice, pd3dCommandList, pstrFileName);
	}
}
CMesh::CMesh(int nPolygons)
{
	m_nPolygons = nPolygons;
	m_ppPolygons = new CPolygon * [nPolygons];
}

CMesh::~CMesh()
{
	if (m_pxmf3Positions) delete[] m_pxmf3Positions;
	if (m_pxmf3Normals) delete[] m_pxmf3Normals;
	if (m_pxmf2TextureCoords) delete[] m_pxmf2TextureCoords;

	if (m_pnIndices) delete[] m_pnIndices;

	if (m_pd3dVertexBufferViews) delete[] m_pd3dVertexBufferViews;

	if (m_pd3dPositionBuffer) m_pd3dPositionBuffer->Release();
	if (m_pd3dNormalBuffer) m_pd3dNormalBuffer->Release();
	if (m_pd3dTextureCoordBuffer) m_pd3dTextureCoordBuffer->Release();
	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer->Release();

	if (m_pd3dBoneIndexBuffer) m_pd3dBoneIndexBuffer->Release();
	if (m_pd3dBoneWeightBuffer) m_pd3dBoneWeightBuffer->Release();
	if (m_pd3dcbBoneTransforms) m_pd3dcbBoneTransforms->Release();

	if (m_pxu4BoneIndices) delete[] m_pxu4BoneIndices;
	if (m_pxmf4BoneWeights) delete[] m_pxmf4BoneWeights;
	if (m_pxmf4x4BoneTransforms) delete[] m_pxmf4x4BoneTransforms;

	if (m_ppPolygons)
	{
		for (int i = 0; i < m_nPolygons; ++i)
		{
			if (m_ppPolygons[i]) delete m_ppPolygons[i];
		}
		delete[] m_ppPolygons;
		m_ppPolygons = nullptr;
	}
}

void CMesh::ReleaseUploadBuffers() 
{
	if (m_pd3dPositionUploadBuffer) m_pd3dPositionUploadBuffer->Release();
	if (m_pd3dNormalUploadBuffer) m_pd3dNormalUploadBuffer->Release();
	if (m_pd3dTextureCoordUploadBuffer) m_pd3dTextureCoordUploadBuffer->Release();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();
	if (m_pd3dBoneIndexUploadBuffer) m_pd3dBoneIndexUploadBuffer->Release();
	if (m_pd3dBoneWeightUploadBuffer) m_pd3dBoneWeightUploadBuffer->Release();

	m_pd3dPositionUploadBuffer = NULL;
	m_pd3dNormalUploadBuffer = NULL;
	m_pd3dTextureCoordUploadBuffer = NULL;
	m_pd3dIndexUploadBuffer = NULL;
	m_pd3dBoneIndexUploadBuffer = NULL;
	m_pd3dBoneWeightUploadBuffer = NULL;
};

void CMesh::Render(ID3D12GraphicsCommandList *pd3dCommandList)
{
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
	pd3dCommandList->IASetVertexBuffers(m_nSlot, m_nVertexBufferViews, m_pd3dVertexBufferViews);
	if (m_bSkinnedMesh && m_pd3dcbBoneTransforms)
	{
		pd3dCommandList->SetGraphicsRootConstantBufferView(4, m_pd3dcbBoneTransforms->GetGPUVirtualAddress());
	}
	if (m_pd3dIndexBuffer)
	{
		pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
	}
	else
	{
		pd3dCommandList->DrawInstanced(m_nVertices, 1, m_nOffset, 0);
	}
}

void CMesh::LoadMeshFromOBJ(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) return;

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	struct Vertex { XMFLOAT3 pos; XMFLOAT3 normal; };
	std::vector<Vertex> vertices;
	std::vector<UINT> indices;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string prefix;
		iss >> prefix;

		if (prefix == "v") {
			float x, y, z;
			iss >> x >> y >> z;
			positions.emplace_back(x, y, z);
		}
		else if (prefix == "vn") {
			float x, y, z;
			iss >> x >> y >> z;
			normals.emplace_back(x, y, z);
		}
		else if (prefix == "f") {
			for (int i = 0; i < 3; ++i) {
				std::string token;
				iss >> token;
				std::istringstream tokenStream(token);
				std::string vIdx, vtIdx, vnIdx;
				std::getline(tokenStream, vIdx, '/');
				std::getline(tokenStream, vtIdx, '/');
				std::getline(tokenStream, vnIdx, '/');

				int v = std::stoi(vIdx) - 1;
				int vn = vnIdx.empty() ? -1 : (std::stoi(vnIdx) - 1);

				Vertex vert;
				vert.pos = positions[v];
				vert.normal = (vn >= 0) ? normals[vn] : XMFLOAT3(0, 1, 0); // 기본 normal

				// 중복 제거 없이 매 face마다 새로 추가
				vertices.push_back(vert);
				indices.push_back(static_cast<UINT>(vertices.size() - 1));
			}
		}
	}
	file.close();

	m_nVertices = static_cast<UINT>(vertices.size());
	m_nIndices = static_cast<UINT>(indices.size());

	// 정점 데이터 저장 (위치 + 노멀)
	struct VertexBufferData { XMFLOAT3 pos; XMFLOAT3 normal; };
	VertexBufferData* vbData = new VertexBufferData[m_nVertices];
	for (UINT i = 0; i < m_nVertices; ++i) {
		vbData[i].pos = vertices[i].pos;
		vbData[i].normal = vertices[i].normal;
	}

	// 기존 m_pxmf3Positions만 유지 필요 시 (아래와 같이 위치만 복사)
	if (m_pxmf3Positions) delete[] m_pxmf3Positions;
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];
	for (UINT i = 0; i < m_nVertices; ++i) m_pxmf3Positions[i] = vertices[i].pos;

	if (m_pxmf3Normals) delete[] m_pxmf3Normals;
	m_pxmf3Normals = new XMFLOAT3[m_nVertices];
	for (UINT i = 0; i < m_nVertices; ++i) m_pxmf3Normals[i] = vertices[i].normal;

	if (m_pnIndices) delete[] m_pnIndices;
	m_pnIndices = new UINT[m_nIndices];
	memcpy(m_pnIndices, indices.data(), sizeof(UINT) * m_nIndices);

	// ===== GPU 리소스 생성 =====
	UINT vbSize = sizeof(VertexBufferData) * m_nVertices;
	m_pd3dPositionBuffer = CreateBufferResource(device, cmdList, vbData, vbSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

	m_nVertexBufferViews = 1;
	m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
	m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VertexBufferData);
	m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

	// 인덱스 버퍼
	UINT ibSize = sizeof(UINT) * m_nIndices;
	m_pd3dIndexBuffer = CreateBufferResource(device, cmdList, m_pnIndices, ibSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = ibSize;

	// OBB 계산은 위치 정보만 사용 (원본 유지)
	XMFLOAT3 min = positions[0], max = positions[0];
	for (const auto& v : positions) {
		if (v.x < min.x) min.x = v.x;
		if (v.y < min.y) min.y = v.y;
		if (v.z < min.z) min.z = v.z;

		if (v.x > max.x) max.x = v.x;
		if (v.y > max.y) max.y = v.y;
		if (v.z > max.z) max.z = v.z;
	}

	XMFLOAT3 center = {
		(min.x + max.x) * 0.5f,
		(min.y + max.y) * 0.5f,
		(min.z + max.z) * 0.5f
	};
	XMFLOAT3 extent = {
		(max.x - min.x) * 0.5f,
		(max.y - min.y) * 0.5f,
		(max.z - min.z) * 0.5f
	};

	m_xmOOBB = BoundingOrientedBox(center, extent, XMFLOAT4(0, 0, 0, 1));

	delete[] vbData;
}

void CMesh::LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename)
{
	FbxManager* pManager = FbxManager::Create();
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	FbxImporter* importer = FbxImporter::Create(pManager, "");
	if (!importer->Initialize(filename, -1, pManager->GetIOSettings())) {
		printf("[FBX] Failed to load file: %s\n", filename);
		importer->Destroy();
		pManager->Destroy();
		return;
	}

	FbxScene* scene = FbxScene::Create(pManager, "scene");
	importer->Import(scene);
	importer->Destroy();

	FbxGeometryConverter converter(pManager);
	converter.Triangulate(scene, true);

	// 단위 및 좌표계 보정
	FbxAxisSystem sceneAxis = scene->GetGlobalSettings().GetAxisSystem();
	FbxAxisSystem::DirectX.ConvertScene(scene);
	FbxSystemUnit::m.ConvertScene(scene); // 미터 단위로 통일

	// 메시 노드 탐색
	FbxNode* rootNode = scene->GetRootNode();
	if (!rootNode) { pManager->Destroy(); return; }

	FbxMesh* fbxMesh = nullptr;
	for (int i = 0; i < rootNode->GetChildCount(); ++i)
	{
		FbxNode* node = rootNode->GetChild(i);
		if (node->GetMesh()) { fbxMesh = node->GetMesh(); break; }
	}
	if (!fbxMesh) { pManager->Destroy(); return; }

	// ===== 정점/법선/UV 추출 =====
	int nCtrlPoints = fbxMesh->GetControlPointsCount();
	FbxVector4* ctrlPoints = fbxMesh->GetControlPoints();

	m_nVertices = nCtrlPoints;
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];
	m_pxmf3Normals = new XMFLOAT3[m_nVertices];
	m_pxmf2TextureCoords = new XMFLOAT2[m_nVertices];

	for (int i = 0; i < nCtrlPoints; ++i)
	{
		FbxVector4 p = ctrlPoints[i];
		m_pxmf3Positions[i] = XMFLOAT3((float)p[0], (float)p[1], (float)p[2]);
		m_pxmf3Normals[i] = XMFLOAT3(0, 1, 0);
		m_pxmf2TextureCoords[i] = XMFLOAT2(0, 0);
	}

	// 노멀 (있다면)
	if (fbxMesh->GetElementNormalCount() > 0)
	{
		FbxGeometryElementNormal* elemNormal = fbxMesh->GetElementNormal(0);
		if (elemNormal->GetMappingMode() == FbxGeometryElement::eByControlPoint)
		{
			for (int i = 0; i < nCtrlPoints; ++i)
			{
				FbxVector4 n = elemNormal->GetDirectArray().GetAt(i);
				m_pxmf3Normals[i] = XMFLOAT3((float)n[0], (float)n[1], (float)n[2]);
			}
		}
	}

	// ===== 인덱스 정보 =====
	m_nIndices = fbxMesh->GetPolygonVertexCount();
	m_pnIndices = new UINT[m_nIndices];
	for (UINT i = 0; i < m_nIndices; ++i)
		m_pnIndices[i] = static_cast<UINT>(fbxMesh->GetPolygonVertices()[i]);

	// ===== 본 스켈레톤 정보 =====
	m_Bones.clear();
	m_BoneNameToIndex.clear();

	std::vector<std::vector<std::pair<int, float>>> ctrlPointWeights(nCtrlPoints);

	int skinCount = fbxMesh->GetDeformerCount(FbxDeformer::eSkin);
	for (int s = 0; s < skinCount; ++s)
	{
		FbxSkin* skin = static_cast<FbxSkin*>(fbxMesh->GetDeformer(s, FbxDeformer::eSkin));
		if (!skin) continue;

		int clusterCount = skin->GetClusterCount();
		for (int c = 0; c < clusterCount; ++c)
		{
			FbxCluster* cluster = skin->GetCluster(c);
			if (!cluster->GetLink()) continue;

			std::string boneName = cluster->GetLink()->GetName();
			int boneIndex = (int)m_Bones.size();

			// 본 추가
			Bone bone;
			bone.name = boneName;
			bone.parentIndex = -1; // 이후에 Animator가 부모-자식 연결을 처리
			FbxAMatrix matrix;
			cluster->GetTransformLinkMatrix(matrix);
			for (int i = 0; i < 4; ++i)
				for (int j = 0; j < 4; ++j)
					bone.offsetMatrix.m[i][j] = (float)matrix.Get(i, j);

			m_Bones.push_back(bone);
			m_BoneNameToIndex[boneName] = boneIndex;

			// 이 본이 영향 주는 정점
			int* indices = cluster->GetControlPointIndices();
			double* weights = cluster->GetControlPointWeights();
			int count = cluster->GetControlPointIndicesCount();
			for (int i = 0; i < count; ++i)
			{
				int idx = indices[i];
				double w = weights[i];
				if (idx >= 0 && idx < nCtrlPoints)
					ctrlPointWeights[idx].push_back({ boneIndex, (float)w });
			}
		}
	}

	// ===== 정점당 4개 본 제한 및 정규화 =====
	m_pxu4BoneIndices = new XMUINT4[nCtrlPoints];
	m_pxmf4BoneWeights = new XMFLOAT4[nCtrlPoints];

	for (int i = 0; i < nCtrlPoints; ++i)
	{
		auto& list = ctrlPointWeights[i];
		std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.second > b.second; });
		if (list.size() > 4) list.resize(4);

		XMUINT4 bi = { 0,0,0,0 };
		XMFLOAT4 bw = { 0,0,0,0 };

		float sum = 0.0f;
		for (size_t k = 0; k < list.size(); ++k) sum += list[k].second;
		if (sum == 0) sum = 1.0f;
		for (size_t k = 0; k < list.size(); ++k)
		{
			bi.x = (k > 0) ? bi.x : list[k].first;
			if (k == 1) bi.y = list[k].first;
			if (k == 2) bi.z = list[k].first;
			if (k == 3) bi.w = list[k].first;

			if (k == 0) bw.x = list[k].second / sum;
			if (k == 1) bw.y = list[k].second / sum;
			if (k == 2) bw.z = list[k].second / sum;
			if (k == 3) bw.w = list[k].second / sum;
		}
		m_pxu4BoneIndices[i] = bi;
		m_pxmf4BoneWeights[i] = bw;
	}

	EnableSkinning((int)m_Bones.size());

	// ===== GPU 리소스 생성 =====
	struct VertexSkinned { XMFLOAT3 pos; XMFLOAT3 normal; XMUINT4 bi; XMFLOAT4 bw; };
	std::vector<VertexSkinned> vtx(m_nVertices);
	for (UINT i = 0; i < m_nVertices; ++i)
	{
		vtx[i].pos = m_pxmf3Positions[i];
		vtx[i].normal = m_pxmf3Normals[i];
		vtx[i].bi = m_pxu4BoneIndices[i];
		vtx[i].bw = m_pxmf4BoneWeights[i];
	}

	UINT vbSize = sizeof(VertexSkinned) * m_nVertices;
	m_pd3dPositionBuffer = CreateBufferResource(device, cmdList, vtx.data(), vbSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

	m_nVertexBufferViews = 1;
	m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
	m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VertexSkinned);
	m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

	// 인덱스 버퍼
	UINT ibSize = sizeof(UINT) * m_nIndices;
	m_pd3dIndexBuffer = CreateBufferResource(device, cmdList, m_pnIndices, ibSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = ibSize;

	// 바운딩 박스 계산
	XMFLOAT3 min = m_pxmf3Positions[0], max = m_pxmf3Positions[0];
	for (UINT i = 0; i < m_nVertices; ++i)
	{
		XMFLOAT3 v = m_pxmf3Positions[i];
		if (v.x < min.x) min.x = v.x;
		if (v.y < min.y) min.y = v.y;
		if (v.z < min.z) min.z = v.z;
		if (v.x > max.x) max.x = v.x;
		if (v.y > max.y) max.y = v.y;
		if (v.z > max.z) max.z = v.z;
	}
	XMFLOAT3 center = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
	XMFLOAT3 extent = { (max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f, (max.z - min.z) * 0.5f };
	m_xmOOBB = BoundingOrientedBox(center, extent, XMFLOAT4(0, 0, 0, 1));

	pManager->Destroy();
}

void CMesh::EnableSkinning(int nBones)
{
	m_bSkinnedMesh = true;
	m_pxmf4x4BoneTransforms = new XMFLOAT4X4[nBones];
	for (int i = 0; i < nBones; ++i)
		XMStoreFloat4x4(&m_pxmf4x4BoneTransforms[i], XMMatrixIdentity());
}


void CMesh::SetPolygon(int nIndex, CPolygon* pPolygon)
{
	if ((0 <= nIndex) && (nIndex < m_nPolygons)) m_ppPolygons[nIndex] = pPolygon;
}

int CMesh::CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance)
{
	int nHits = 0;
	float fNearestHit = FLT_MAX;

	for (UINT i = 0; i < m_nIndices; i += 3)
	{
		XMVECTOR v0 = XMLoadFloat3(&m_pxmf3Positions[m_pnIndices[i]]);
		XMVECTOR v1 = XMLoadFloat3(&m_pxmf3Positions[m_pnIndices[i + 1]]);
		XMVECTOR v2 = XMLoadFloat3(&m_pxmf3Positions[m_pnIndices[i + 2]]);

		float fDist = 0.0f;
		if (TriangleTests::Intersects(xmvPickRayOrigin, xmvPickRayDirection, v0, v1, v2, fDist))
		{
			if (fDist < fNearestHit)
			{
				fNearestHit = fDist;
				nHits++;
				if (pfNearHitDistance) *pfNearHitDistance = fNearestHit;
			}
		}
	}

	return nHits;
}
BOOL CMesh::RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance)
{
	float fHitDistance;
	BOOL bIntersected = TriangleTests::Intersects(xmRayOrigin, xmRayDirection, v0, v1, v2, fHitDistance);
	if (bIntersected && (fHitDistance < *pfNearHitDistance)) *pfNearHitDistance = fHitDistance;

	return(bIntersected);
}