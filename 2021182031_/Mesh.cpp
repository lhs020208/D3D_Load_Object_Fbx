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
struct AxisFix {
    bool flipX = false;   // 좌우
    bool flipY = false;   // 상하 반전
    bool flipZ = true;    // RH→LH 전환
    bool swapYZ = false;  // 필요 시 Z-up→Y-up 회전
};

static inline void ApplyAxisFix(XMFLOAT3& p, XMFLOAT3& n, AxisFix fix, bool& flipWinding)
{
    if (fix.swapYZ) {
        float py = p.y, pz = p.z; p.y = pz;  p.z = -py;
        float ny = n.y, nz = n.z; n.y = nz;  n.z = -ny;
    }

    if (fix.flipZ) { p.z = -p.z; n.z = -n.z; flipWinding = !flipWinding; }
    if (fix.flipY) { p.y = -p.y; n.y = -n.y; flipWinding = !flipWinding; } // ← 추가
    if (fix.flipX) { p.x = -p.x; n.x = -n.x; flipWinding = !flipWinding; }
}

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

void CMesh::Render(ID3D12GraphicsCommandList* cmdList)
{
    // 1) 스키닝 메시일 경우만 애니메이션 처리
    if (m_bSkinnedMesh)
    {
        // Animator가 존재할 때만 업데이트
        if (m_pAnimator)
        {
            m_pAnimator->Update(1.0 / 60.0, m_Bones, m_pxmf4x4BoneTransforms);

            D3D12_RANGE readRange{ 0, 0 };
            XMFLOAT4X4* mapped = nullptr;
            m_pd3dcbBoneTransforms->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
            memcpy(mapped, m_pxmf4x4BoneTransforms, sizeof(XMFLOAT4X4) * m_Bones.size());
            m_pd3dcbBoneTransforms->Unmap(0, nullptr);

            cmdList->SetGraphicsRootConstantBufferView(4, m_pd3dcbBoneTransforms->GetGPUVirtualAddress());
        }
        else
        {
            // Animator가 없을 경우: 본이 있지만 애니메이션은 없음 → 기본 본행렬 그대로
            if (m_pd3dcbBoneTransforms)
                cmdList->SetGraphicsRootConstantBufferView(4, m_pd3dcbBoneTransforms->GetGPUVirtualAddress());
        }
    }

    // 2) 기존 메시 렌더링 루틴
    cmdList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
    cmdList->IASetVertexBuffers(m_nSlot, m_nVertexBufferViews, m_pd3dVertexBufferViews);

    if (m_pd3dIndexBuffer)
    {
        cmdList->IASetIndexBuffer(&m_d3dIndexBufferView);
        cmdList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
    }
    else
    {
        cmdList->DrawInstanced(m_nVertices, 1, 0, 0);
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
/*
// 단일노드 객체는 가져와지던 LoadMeshFromFBX
void CMesh::LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename)
{
    FbxManager* pManager = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
    pManager->SetIOSettings(ios);

    FbxImporter* importer = FbxImporter::Create(pManager, "");
    if (!importer->Initialize(filename, -1, pManager->GetIOSettings()))
    {
        OutputDebugStringA("[FBX] Importer 초기화 실패\n");
        importer->Destroy();
        pManager->Destroy();
        return;
    }

    FbxScene* scene = FbxScene::Create(pManager, "scene");
    importer->Import(scene);
    importer->Destroy();

    FbxGeometryConverter converter(pManager);
    converter.Triangulate(scene, true);

    FbxNode* rootNode = scene->GetRootNode();
    if (!rootNode)
    {
        OutputDebugStringA("[FBX] Root 노드 없음\n");
        pManager->Destroy();
        return;
    }

    // ===== 메시 찾기 =====
    FbxMesh* fbxMesh = nullptr;
    std::function<FbxMesh* (FbxNode*)> FindMeshRecursive = [&](FbxNode* node) -> FbxMesh*
    {
        if (node->GetMesh()) return node->GetMesh();
        for (int i = 0; i < node->GetChildCount(); ++i)
            if (auto found = FindMeshRecursive(node->GetChild(i))) return found;
        return nullptr;
    };
    fbxMesh = FindMeshRecursive(rootNode);
    if (!fbxMesh)
    {
        OutputDebugStringA("[FBX] 메시 노드 없음\n");
        pManager->Destroy();
        return;
    }

    // ===== 메시 읽기 =====
    struct Vertex { XMFLOAT3 pos; XMFLOAT3 normal; };
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;

    int polygonCount = fbxMesh->GetPolygonCount();
    vertices.reserve(polygonCount * 3);
    indices.reserve(polygonCount * 3);

    for (int p = 0; p < polygonCount; ++p)
    {
        for (int v = 0; v < 3; ++v)
        {
            int ctrlIdx = fbxMesh->GetPolygonVertex(p, v);
            FbxVector4 cp = fbxMesh->GetControlPointAt(ctrlIdx);
            FbxVector4 n;
            fbxMesh->GetPolygonVertexNormal(p, v, n);

            Vertex vert;
            vert.pos = XMFLOAT3((float)cp[0], (float)cp[1], (float)cp[2]);
            vert.normal = XMFLOAT3((float)n[0], (float)n[1], (float)n[2]);

            vertices.push_back(vert);
            indices.push_back((UINT)vertices.size() - 1);
        }
    }

    m_nVertices = (UINT)vertices.size();
    m_nIndices = (UINT)indices.size();

    // ===== 본 정보 추출 =====
    m_Bones.clear();
    m_BoneNameToIndex.clear();

    std::function<void(FbxNode*, int)> ExtractBones = [&](FbxNode* node, int parentIndex)
    {
        if (node->GetNodeAttribute() &&
            node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
        {
            Bone bone;
            bone.name = node->GetName();
            bone.parentIndex = parentIndex;

            FbxAMatrix mat = node->EvaluateGlobalTransform();
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    bone.offsetMatrix.m[i][j] = (float)mat.Get(i, j);

            int boneIndex = (int)m_Bones.size();
            m_BoneNameToIndex[bone.name] = boneIndex;
            m_Bones.push_back(bone);
        }

        int thisIndex = (node->GetNodeAttribute() && node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            ? m_BoneNameToIndex[node->GetName()] : parentIndex;

        for (int i = 0; i < node->GetChildCount(); ++i)
            ExtractBones(node->GetChild(i), thisIndex);
    };

    ExtractBones(rootNode, -1);

    // ===== GPU 버퍼 생성 (기존 동일) =====
    struct VBData { XMFLOAT3 pos; XMFLOAT3 normal; };
    std::unique_ptr<VBData[]> vbData(new VBData[m_nVertices]);
    for (UINT i = 0; i < m_nVertices; ++i)
    {
        vbData[i].pos = vertices[i].pos;
        vbData[i].normal = vertices[i].normal;
    }

    UINT vbSize = sizeof(VBData) * m_nVertices;
    m_pd3dPositionBuffer = CreateBufferResource(device, cmdList, vbData.get(), vbSize,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

    m_nVertexBufferViews = 1;
    m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
    m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
    m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VBData);
    m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

    UINT ibSize = sizeof(UINT) * m_nIndices;
    m_pd3dIndexBuffer = CreateBufferResource(device, cmdList, indices.data(), ibSize,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

    m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
    m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_d3dIndexBufferView.SizeInBytes = ibSize;

    // ===== 로그 출력 =====
    std::ostringstream log;
    log << "[FBX] Mesh Loaded: " << filename << "\n"
        << "   Vertices: " << m_nVertices << "\n"
        << "   Indices : " << m_nIndices << "\n"
        << "   Bones   : " << m_Bones.size() << "\n";
    OutputDebugStringA(log.str().c_str());

    pManager->Destroy();
}
*/
void CMesh::LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename)
{
    // ========= 0) FBX 초기화 =========
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings())) { imp->Destroy(); mgr->Destroy(); return; }

    FbxScene* scene = FbxScene::Create(mgr, "scene");
    imp->Import(scene);
    imp->Destroy();

    // ========= 1) 좌표계/단위 정규화 =========
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // ========= 2) Triangulate =========
    {
        FbxGeometryConverter conv(mgr);
        conv.Triangulate(scene, /*replace*/ true);
    }

    // ========= 3) 모든 Mesh 수집 =========
    std::vector<FbxMesh*> meshes;
    auto CollectMeshes = [&](FbxNode* root)
        {
            std::function<void(FbxNode*)> dfs = [&](FbxNode* n)
                {
                    if (!n) return;
                    if (auto* m = n->GetMesh()) meshes.push_back(m);
                    for (int i = 0; i < n->GetChildCount(); ++i) dfs(n->GetChild(i));
                };
            if (root) dfs(root);
        };
    FbxNode* root = scene->GetRootNode();
    if (!root) { mgr->Destroy(); return; }
    CollectMeshes(root);
    if (meshes.empty()) { mgr->Destroy(); return; }

    // ========= 4) 본 계층 수집 =========
    m_Bones.clear();
    m_BoneNameToIndex.clear();

    std::function<void(FbxNode*, int)> ExtractBones = [&](FbxNode* node, int parent)
        {
            if (!node) return;
            int self = parent;
            if (auto* a = node->GetNodeAttribute())
            {
                if (a->GetAttributeType() == FbxNodeAttribute::eSkeleton)
                {
                    Bone b{};
                    b.name = node->GetName();
                    b.parentIndex = parent;

                    // 일단 글로벌(바인드) 변환 저장(필요 시 이후 재설정)
                    FbxAMatrix g = node->EvaluateGlobalTransform();
                    for (int r = 0; r < 4; ++r)
                        for (int c = 0; c < 4; ++c)
                            b.offsetMatrix.m[r][c] = (float)g.Get(r, c);

                    self = (int)m_Bones.size();
                    m_BoneNameToIndex[b.name] = self;
                    m_Bones.push_back(b);
                }
            }
            for (int i = 0; i < node->GetChildCount(); ++i) ExtractBones(node->GetChild(i), self);
        };
    ExtractBones(root, -1);

    // ========= 4-1) 스킨 클러스터로 본 offset(=링크 바인드) 보정 & CP별 가중치 수집 =========
    // 메시마다 다른 스킨이 있을 수 있으므로, CP(컨트롤 포인트) -> (bone, weight) 모음
    struct WPair { int bone; float w; };
    using CPWeights = std::vector<std::vector<WPair>>; // per-mesh: [cpIndex] -> list of (bone,w)
    std::vector<CPWeights> allMeshCPWeights(meshes.size());

    bool anySkinned = false;

    for (size_t mi = 0; mi < meshes.size(); ++mi)
    {
        FbxMesh* mesh = meshes[mi];
        if (!mesh) continue;
        allMeshCPWeights[mi].resize(mesh->GetControlPointsCount());

        int dCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int d = 0; d < dCount; ++d)
        {
            auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
            if (!skin) continue;
            anySkinned = true;

            int cCount = skin->GetClusterCount();
            for (int c = 0; c < cCount; ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                if (!cluster) continue;

                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                auto it = m_BoneNameToIndex.find(linkNode->GetName());
                if (it == m_BoneNameToIndex.end()) continue;
                int boneIdx = it->second;

                // 링크 바인드 매트릭스로 offsetMatrix 업데이트
                // 일부 SDK/파일 조합에서 GetTransformLinkMatrix() 크래시 보고 있어 안전 경로 사용
                FbxAMatrix linkBind;        // bone global at bind-pose
                FbxAMatrix meshBind;        // mesh global at bind-pose (필요 시 사용)
                if (cluster->GetTransformLinkMatrix(linkBind)) {
                    // 선택: meshBind를 쓰고 싶다면 아래 줄도 받자
                    // cluster->GetTransformMatrix(meshBind);

                    // 역-바인드 저장
                    FbxAMatrix invLinkBind = linkBind.Inverse();
                    XMFLOAT4X4 invBindXM;
                    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
                        invBindXM.m[r][c] = (float)invLinkBind.Get(r, c);
                    m_Bones[boneIdx].offsetMatrix = invBindXM;
                }
                else {
                    // 폴백: 노드의 바인드 포즈(또는 현재 글로벌)로부터 유사 역-바인드
                    FbxAMatrix fallback = linkNode->EvaluateGlobalTransform();
                    FbxAMatrix inv = fallback.Inverse();
                    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
                        m_Bones[boneIdx].offsetMatrix.m[r][c] = (float)inv.Get(r, c);
                }

                int cpCount = cluster->GetControlPointIndicesCount();
                int* cpIdx = cluster->GetControlPointIndices();
                double* cpW = cluster->GetControlPointWeights();
                for (int i = 0; i < cpCount; ++i)
                {
                    int idx = cpIdx[i];
                    float w = (float)cpW[i];
                    if (idx >= 0 && idx < mesh->GetControlPointsCount() && w > 0.0f)
                        allMeshCPWeights[mi][idx].push_back({ boneIdx, w });
                }
            }
        }
    }

    // ========= 5) 정점/인덱스 병합 (pos/normal/uv + bone index/weight) =========
    struct VTX { XMFLOAT3 pos; XMFLOAT3 nrm; XMFLOAT2 uv; };
    std::vector<VTX> outV;
    std::vector<UINT> outI;
    std::vector<XMUINT4>  outBI; // per-vertex bone indices
    std::vector<XMFLOAT4> outBW; // per-vertex bone weights

    auto ToXM3 = [](const FbxVector4& v) { return XMFLOAT3((float)v[0], (float)v[1], (float)v[2]); };
    auto ToXM2 = [](const FbxVector2& v) { return XMFLOAT2((float)v[0], (float)v[1]); };

    for (size_t mi = 0; mi < meshes.size(); ++mi)
    {
        FbxMesh* mesh = meshes[mi];
        if (!mesh) continue;
        if (mesh->GetPolygonCount() == 0 || mesh->GetControlPointsCount() == 0) continue;

        FbxNode* node = mesh->GetNode();
        FbxAMatrix global = node ? node->EvaluateGlobalTransform() : FbxAMatrix();

        // 지오메트리 오프셋 포함
        FbxAMatrix geo;
        if (node) {
            geo.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
            geo.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
            geo.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
        }
        FbxAMatrix xform = global * geo;

        bool flip = (xform.Determinant() < 0.0);
        const int polyCount = mesh->GetPolygonCount();

        // UV 채널 준비 (0번 UVSet 사용)
        FbxStringList uvSets;
        mesh->GetUVSetNames(uvSets);
        const char* uvSetName = (uvSets.GetCount() > 0) ? uvSets.GetStringAt(0) : nullptr;


        for (int p = 0; p < polyCount; ++p)
        {
            int ord[3] = { 0,1,2 };
            if (flip) std::swap(ord[1], ord[2]);

            for (int v = 0; v < 3; ++v)
            {
                int corner = ord[v];
                int cpIdx = mesh->GetPolygonVertex(p, corner);

                // 위치/법선
                FbxVector4 cp = mesh->GetControlPointAt(cpIdx);
                FbxVector4 pw = xform.MultT(cp); // w=1

                FbxVector4 n(0, 1, 0, 0);
                mesh->GetPolygonVertexNormal(p, corner, n);
                FbxVector4 nw = xform.MultT(FbxVector4(n[0], n[1], n[2], 0.0)); // w=0
                double L = sqrt(nw[0] * nw[0] + nw[1] * nw[1] + nw[2] * nw[2]);
                if (L > 1e-12) { nw[0] /= L; nw[1] /= L; nw[2] /= L; }

                // UV
                XMFLOAT2 uv{ 0.0f, 0.0f };
                if (uvSetName)
                {
                    FbxVector2 uvv;
                    bool unmapped = false;
                    if (mesh->GetPolygonVertexUV(p, corner, uvSetName, uvv, unmapped))
                        uv = ToXM2(uvv);
                }

                // 본 인덱스/가중치 (컨트롤 포인트에서 가져와 상위 4개 뽑기)
                XMUINT4 bi = { 0,0,0,0 };
                XMFLOAT4 bw = { 0,0,0,0 };
                if (!allMeshCPWeights[mi].empty() && cpIdx >= 0 && cpIdx < (int)allMeshCPWeights[mi].size())
                {
                    auto& list = allMeshCPWeights[mi][cpIdx];
                    if (!list.empty())
                    {
                        // 상위 4개 선택
                        std::partial_sort(list.begin(),
                            list.begin() + std::min<size_t>(4, list.size()),
                            list.end(),
                            [](const WPair& a, const WPair& b) { return a.w > b.w; });

                        float sum = 0.0f;
                        for (int k = 0; k < 4 && k < (int)list.size(); ++k)
                        {
                            (&bi.x)[k] = (UINT)list[k].bone;
                            (&bw.x)[k] = list[k].w;
                            sum += list[k].w;
                        }
                        if (sum > 0.0f) { bw.x /= sum; bw.y /= sum; bw.z /= sum; bw.w /= sum; }
                    }
                }

                // push vertex
                VTX vv;
                vv.pos = ToXM3(pw);
                vv.nrm = ToXM3(nw);
                vv.uv = uv;

                outV.push_back(vv);
                outBI.push_back(bi);
                outBW.push_back(bw);

                outI.push_back((UINT)outV.size() - 1);
            }
        }
    }

    // ========= 6) CPU 보관/ GPU 업로드 =========
    // 기존 CPU 배열 갱신
    m_nVertices = (UINT)outV.size();
    m_nIndices = (UINT)outI.size();

    if (m_pxmf3Positions) delete[] m_pxmf3Positions;
    if (m_pxmf3Normals)   delete[] m_pxmf3Normals;
    if (m_pxmf2TextureCoords) delete[] m_pxmf2TextureCoords;
    if (m_pnIndices)      delete[] m_pnIndices;

    m_pxmf3Positions = (m_nVertices ? new XMFLOAT3[m_nVertices] : nullptr);
    m_pxmf3Normals = (m_nVertices ? new XMFLOAT3[m_nVertices] : nullptr);
    m_pxmf2TextureCoords = (m_nVertices ? new XMFLOAT2[m_nVertices] : nullptr);
    m_pnIndices = (m_nIndices ? new UINT[m_nIndices] : nullptr);

    for (UINT i = 0; i < m_nVertices; ++i) {
        m_pxmf3Positions[i] = outV[i].pos;
        m_pxmf3Normals[i] = outV[i].nrm;
        m_pxmf2TextureCoords[i] = outV[i].uv;
    }
    if (m_nIndices) memcpy(m_pnIndices, outI.data(), sizeof(UINT) * m_nIndices);

    // ====== 메인 VB: pos+normal+uv ======
    struct VB { XMFLOAT3 pos; XMFLOAT3 nrm; XMFLOAT2 uv; };
    if (m_pd3dVertexBufferViews) { delete[] m_pd3dVertexBufferViews; m_pd3dVertexBufferViews = nullptr; }

    if (m_nVertices) {
        std::unique_ptr<VB[]> vb(new VB[m_nVertices]);
        for (UINT i = 0; i < m_nVertices; ++i) {
            vb[i].pos = m_pxmf3Positions[i];
            vb[i].nrm = m_pxmf3Normals[i];
            vb[i].uv = m_pxmf2TextureCoords[i];
        }
        UINT vbSize = sizeof(VB) * m_nVertices;

        m_pd3dPositionBuffer = CreateBufferResource(
            device, cmdList, vb.get(), vbSize,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

        // ====== 인덱스 버퍼 ======
        if (m_nIndices) {
            UINT ibSize = sizeof(UINT) * m_nIndices;
            m_pd3dIndexBuffer = CreateBufferResource(
                device, cmdList, m_pnIndices, ibSize,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

            m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
            m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            m_d3dIndexBufferView.SizeInBytes = ibSize;
        }

        // ====== 본 인덱스/가중치 버퍼 (VB 슬롯 1, 2) ======
        if (!outBI.empty() && !outBW.empty())
        {
            // CPU 보관
            if (m_pxu4BoneIndices) delete[] m_pxu4BoneIndices;
            if (m_pxmf4BoneWeights) delete[] m_pxmf4BoneWeights;
            m_pxu4BoneIndices = new XMUINT4[m_nVertices];
            m_pxmf4BoneWeights = new XMFLOAT4[m_nVertices];
            memcpy(m_pxu4BoneIndices, outBI.data(), sizeof(XMUINT4) * m_nVertices);
            memcpy(m_pxmf4BoneWeights, outBW.data(), sizeof(XMFLOAT4) * m_nVertices);

            // GPU 생성
            UINT biSize = sizeof(XMUINT4) * m_nVertices;
            UINT bwSize = sizeof(XMFLOAT4) * m_nVertices;

            m_pd3dBoneIndexBuffer = CreateBufferResource(
                device, cmdList, m_pxu4BoneIndices, biSize,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBoneIndexUploadBuffer);

            m_pd3dBoneWeightBuffer = CreateBufferResource(
                device, cmdList, m_pxmf4BoneWeights, bwSize,
                D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dBoneWeightUploadBuffer);

            // VB 뷰 배열 구성: slot0(main), slot1(boneIndex), slot2(boneWeight)
            m_nVertexBufferViews = 3;
            m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[m_nVertexBufferViews];

            // slot 0
            m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
            m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VB);
            m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

            // slot 1 (indices)
            m_pd3dVertexBufferViews[1].BufferLocation = m_pd3dBoneIndexBuffer->GetGPUVirtualAddress();
            m_pd3dVertexBufferViews[1].StrideInBytes = sizeof(XMUINT4);
            m_pd3dVertexBufferViews[1].SizeInBytes = biSize;

            // slot 2 (weights)
            m_pd3dVertexBufferViews[2].BufferLocation = m_pd3dBoneWeightBuffer->GetGPUVirtualAddress();
            m_pd3dVertexBufferViews[2].StrideInBytes = sizeof(XMFLOAT4);
            m_pd3dVertexBufferViews[2].SizeInBytes = bwSize;

            // 스키닝 활성화 + 본 cbuffer 준비
            EnableSkinning(device, (int)m_Bones.size());
        }
        else
        {
            // 스키닝 데이터가 없으면 단일 VB만
            m_nVertexBufferViews = 1;
            m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
            m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
            m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VB);
            m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

            m_bSkinnedMesh = false;
        }
    }

    // ========= 7) OBB =========
    if (m_nVertices > 0) {
        XMFLOAT3 mn = m_pxmf3Positions[0], mx = m_pxmf3Positions[0];
        for (UINT i = 1; i < m_nVertices; ++i) {
            auto& v = m_pxmf3Positions[i];
            if (v.x < mn.x) mn.x = v.x; if (v.y < mn.y) mn.y = v.y; if (v.z < mn.z) mn.z = v.z;
            if (v.x > mx.x) mx.x = v.x; if (v.y > mx.y) mx.y = v.y; if (v.z > mx.z) mx.z = v.z;
        }
        XMFLOAT3 c{ (mn.x + mx.x) * 0.5f,(mn.y + mx.y) * 0.5f,(mn.z + mx.z) * 0.5f };
        XMFLOAT3 e{ (mx.x - mn.x) * 0.5f,(mx.y - mn.y) * 0.5f,(mx.z - mn.z) * 0.5f };
        m_xmOOBB = BoundingOrientedBox(c, e, XMFLOAT4(0, 0, 0, 1));
    }

    // ========= 로그 =========
    std::ostringstream log;
    log << "[FBX] Mesh Loaded: " << filename << "\n"
        << "   MeshParts: " << meshes.size() << "\n"
        << "   Vertices : " << m_nVertices << "\n"
        << "   Indices  : " << m_nIndices << "\n"
        << "   Bones    : " << m_Bones.size() << "\n"
        << "   Skinned  : " << (anySkinned ? "Yes" : "No") << "\n";
    OutputDebugStringA(log.str().c_str());

    mgr->Destroy();
}


void CMesh::EnableSkinning(int nBones)
{
	m_bSkinnedMesh = true;
	m_pxmf4x4BoneTransforms = new XMFLOAT4X4[nBones];
	for (int i = 0; i < nBones; ++i)
		XMStoreFloat4x4(&m_pxmf4x4BoneTransforms[i], XMMatrixIdentity());
}

void CMesh::EnableSkinning(ID3D12Device* device, int nBones)
{
    m_bSkinnedMesh = true;
    m_pxmf4x4BoneTransforms = new XMFLOAT4X4[nBones];
    for (int i = 0; i < nBones; ++i)
        XMStoreFloat4x4(&m_pxmf4x4BoneTransforms[i], XMMatrixIdentity());

    UINT cbSize = sizeof(XMFLOAT4X4) * nBones;
    m_pd3dcbBoneTransforms = CreateBufferResource(
        device, nullptr, nullptr, cbSize,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
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


void CMesh::LoadAnimationFromFBX(const char* filename)
{
    // ========= 1) FBX 초기화 =========
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    FbxScene* scene = FbxScene::Create(mgr, "scene");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings())) { imp->Destroy(); mgr->Destroy(); return; }
    imp->Import(scene);
    imp->Destroy();

    // ========= 2) 좌표계/단위 정규화 (메시와 동일하게) =========
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // ========= 3) 애니메이션 스택/레이어 =========
    if (scene->GetSrcObjectCount<FbxAnimStack>() <= 0) { mgr->Destroy(); return; }
    FbxAnimStack* stack = scene->GetSrcObject<FbxAnimStack>(0);
    scene->SetCurrentAnimationStack(stack);

    FbxAnimLayer* layer = stack->GetMember<FbxAnimLayer>(0);
    if (!layer) { mgr->Destroy(); return; }

    // 시간 범위/샘플 간격 설정 (스택 범위 기준, 프레임레이트는 씬 설정 사용)
    FbxTakeInfo* takeInfo = scene->GetTakeInfo(*(stack->GetName()));
    FbxTime start = takeInfo ? takeInfo->mLocalTimeSpan.GetStart() : stack->LocalStart;
    FbxTime end = takeInfo ? takeInfo->mLocalTimeSpan.GetStop() : stack->LocalStop;

    FbxTime::EMode timeMode = scene->GetGlobalSettings().GetTimeMode();
    double fps = FbxTime::GetFrameRate(timeMode);
    if (fps <= 0.0) fps = 30.0; // fallback
    FbxTime step; step.SetSecondDouble(1.0 / fps);

    // ========= 4) 기존 키프레임 초기화 =========
    for (auto& b : m_Bones) b.keyframes.clear();

    // ========= 5) 본 이름 매칭 후, 프레임 단위 샘플링 =========
    for (auto& bone : m_Bones)
    {
        FbxNode* boneNode = scene->FindNodeByName(bone.name.c_str());
        if (!boneNode) continue;

        for (FbxTime t = start; t <= end; t += step)
        {
            FbxAMatrix M = boneNode->EvaluateGlobalTransform(t);

            Keyframe k;
            k.time = t.GetSecondDouble();
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    k.transform.m[r][c] = (float)M.Get(r, c);

            bone.keyframes.push_back(k);
        }
    }

    // ========= 6) Animator 연결 (없다면 생성) =========
    if (!m_pAnimator) m_pAnimator = new CAnimator();
    // Animator가 뼈대와 키프레임(=m_Bones)에서 보간해 m_pxmf4x4BoneTransforms를 갱신하는 구조라고 가정
    // 필요시 Animator 쪽에 SetBones / SetClips 등의 세터를 호출하도록 수정.
    // 여기서는 m_Bones만 갱신해두면 Render()에서 m_pAnimator->Update(...)가 동작.

    // ========= 로그 =========
    int keyedBones = 0, totalKeys = 0;
    for (auto& b : m_Bones) { if (!b.keyframes.empty()) keyedBones++; totalKeys += (int)b.keyframes.size(); }
    std::ostringstream log;
    log << "[FBX] Anim Loaded: " << filename << "\n"
        << "   Bones with Keys: " << keyedBones << " / " << m_Bones.size() << "\n"
        << "   Total Keyframes : " << totalKeys << "\n"
        << "   FPS             : " << fps << "\n";
    OutputDebugStringA(log.str().c_str());

    mgr->Destroy();
}

