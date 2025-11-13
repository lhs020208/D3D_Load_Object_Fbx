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
    m_pd3dTexture = nullptr;
    m_pd3dTextureUploadBuffer = nullptr;
    m_nTextureDescriptorIndex = UINT_MAX;
}

CMesh::~CMesh()
{
    // ---- Texture Release ----
    if (m_pd3dTexture) {
        m_pd3dTexture->Release();
        m_pd3dTexture = nullptr;
    }
    if (m_pd3dTextureUploadBuffer) {
        m_pd3dTextureUploadBuffer->Release();
        m_pd3dTextureUploadBuffer = nullptr;
    }

    // 만약 DescriptorIndex는 Heap에서 자동 소멸되므로 따로 해제 없음
    m_nTextureDescriptorIndex = UINT_MAX;

    // ---- 기존 Mesh 리소스 해제 ----
    if (m_pxmf3Positions) delete[] m_pxmf3Positions;
    if (m_pxmf3Normals)   delete[] m_pxmf3Normals;
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

    if (m_ppPolygons) {
        for (int i = 0; i < m_nPolygons; ++i) {
            if (m_ppPolygons[i]) delete m_ppPolygons[i];
        }
        delete[] m_ppPolygons;
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
    // 0) FBX 초기화
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings())) { imp->Destroy(); mgr->Destroy(); return; }

    FbxScene* scene = FbxScene::Create(mgr, "scene");
    imp->Import(scene);
    imp->Destroy();

    // 1) 좌표계/단위 정규화
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // 2) Triangulate를 먼저 수행 (※ 이후 포인터가 바뀌므로, '수집'은 나중에!)
    {
        FbxGeometryConverter conv(mgr);
        conv.Triangulate(scene, /*replace*/ true);
    }

    // 3) 모든 Mesh 수집 (Triangulate 이후에!)
    std::vector<FbxMesh*> meshes;
    auto CollectMeshes = [&](FbxNode* root)
        {
            std::function<void(FbxNode*)> dfs = [&](FbxNode* n)
                {
                    if (!n) return;
                    if (auto* m = n->GetMesh()) meshes.push_back(m);
                    for (int i = 0; i < n->GetChildCount(); ++i) dfs(n->GetChild(i));
                };
            dfs(root);
        };
    FbxNode* root = scene->GetRootNode();
    if (!root) { mgr->Destroy(); return; }
    CollectMeshes(root);
    if (meshes.empty()) { mgr->Destroy(); return; }

    // 4) 본 수집
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

                    // 일단 글로벌 트랜스폼 저장 (뒤에서 보정 가능)
                    FbxAMatrix g = node->EvaluateGlobalTransform();
                    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) b.offsetMatrix.m[r][c] = (float)g.Get(r, c);

                    self = (int)m_Bones.size();
                    m_BoneNameToIndex[b.name] = self;
                    m_Bones.push_back(b);
                }
            }
            for (int i = 0; i < node->GetChildCount(); ++i) ExtractBones(node->GetChild(i), self);
        };
    ExtractBones(root, -1);

    // 4-1) 스킨 클러스터 보정: GetTransformLinkMatrix()는 크래시가 잦으므로 안전 대체로 EvaluateGlobalTransform 사용
    for (auto* mesh : meshes)
    {
        if (!mesh) continue;
        int dCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int d = 0; d < dCount; ++d)
        {
            auto* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
            if (!skin) continue;

            int cCount = skin->GetClusterCount();
            for (int c = 0; c < cCount; ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                if (!cluster) continue;

                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                auto it = m_BoneNameToIndex.find(linkNode->GetName());
                if (it == m_BoneNameToIndex.end()) continue;

                // 안전한 대체: 일부 FBX/SDK 조합에서 GetTransformLinkMatrix가 크래시.
                // => 링크 본의 바인드 포즈를 EvaluateGlobalTransform으로 사용.
                FbxAMatrix linkMat = linkNode->EvaluateGlobalTransform();

                Bone& b = m_Bones[it->second];
                for (int r = 0; r < 4; ++r) for (int cc = 0; cc < 4; ++cc) b.offsetMatrix.m[r][cc] = (float)linkMat.Get(r, cc);
            }
        }
    }

    // 5) 모든 메시를 하나의 정적 버퍼로 병합 (pos+normal+uv)
    struct VtxUV { XMFLOAT3 pos; XMFLOAT3 nrm; XMFLOAT2 uv; };
    std::vector<VtxUV> outV;
    std::vector<UINT> outI;

    auto ToXM3 = [](const FbxVector4& v) { return XMFLOAT3((float)v[0], (float)v[1], (float)v[2]); };
    auto ToXM2 = [](const FbxVector2& v) { return XMFLOAT2((float)v[0], (float)v[1]); };

    for (auto* mesh : meshes)
    {
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
        UINT base = (UINT)outV.size();

        // 0번 UVSet 이름 가져오기
        FbxStringList uvSets;
        mesh->GetUVSetNames(uvSets);
        const char* uvSetName = (uvSets.GetCount() > 0) ? uvSets.GetStringAt(0) : nullptr;

        int polyCount = mesh->GetPolygonCount();
        for (int p = 0; p < polyCount; ++p)
        {
            int ord[3] = { 0,1,2 };
            if (flip) std::swap(ord[1], ord[2]);

            for (int i = 0; i < 3; ++i)
            {
                int v = ord[i];
                int cpIdx = mesh->GetPolygonVertex(p, v);
                FbxVector4 cp = mesh->GetControlPointAt(cpIdx);

                // 위치/법선 변환
                FbxVector4 pw = xform.MultT(cp); // w=1
                FbxVector4 n; mesh->GetPolygonVertexNormal(p, v, n);
                FbxVector4 nw = xform.MultT(FbxVector4(n[0], n[1], n[2], 0.0)); // w=0
                double L = sqrt(nw[0] * nw[0] + nw[1] * nw[1] + nw[2] * nw[2]);
                if (L > 1e-12) { nw[0] /= L; nw[1] /= L; nw[2] /= L; }

                // UV 좌표 읽기
                XMFLOAT2 uv{ 0.0f, 0.0f };
                if (uvSetName)
                {
                    FbxVector2 uvv;
                    bool unmapped = false;
                    if (mesh->GetPolygonVertexUV(p, v, uvSetName, uvv, unmapped))
                        uv = ToXM2(uvv);
                }

                outV.push_back({ ToXM3(pw), ToXM3(nw), uv });
                outI.push_back((UINT)outV.size() - 1);
            }
        }
    }

    // 6) CPU 보관/ GPU 업로드
    m_nVertices = (UINT)outV.size();
    m_nIndices = (UINT)outI.size();

    if (m_pxmf3Positions) delete[] m_pxmf3Positions;
    if (m_pxmf3Normals)   delete[] m_pxmf3Normals;
    if (m_pxmf2TextureCoords) delete[] m_pxmf2TextureCoords;
    if (m_pnIndices)      delete[] m_pnIndices;

    m_pxmf3Positions = new XMFLOAT3[m_nVertices];
    m_pxmf3Normals = new XMFLOAT3[m_nVertices];
    m_pxmf2TextureCoords = new XMFLOAT2[m_nVertices];
    m_pnIndices = new UINT[m_nIndices];

    for (UINT i = 0; i < m_nVertices; ++i) {
        m_pxmf3Positions[i] = outV[i].pos;
        m_pxmf3Normals[i] = outV[i].nrm;
        m_pxmf2TextureCoords[i] = outV[i].uv;
    }
    memcpy(m_pnIndices, outI.data(), sizeof(UINT) * m_nIndices);

    // VB 생성
    struct VB { XMFLOAT3 pos; XMFLOAT3 nrm; XMFLOAT2 uv; };
    std::unique_ptr<VB[]> vb(new VB[m_nVertices]);
    for (UINT i = 0; i < m_nVertices; ++i) {
        vb[i].pos = m_pxmf3Positions[i];
        vb[i].nrm = m_pxmf3Normals[i];
        vb[i].uv = m_pxmf2TextureCoords[i];
    }

    UINT vbSize = sizeof(VB) * m_nVertices;
    m_pd3dPositionBuffer = CreateBufferResource(
        device, cmdList, vb.get(), vbSize,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        &m_pd3dPositionUploadBuffer);

    m_nVertexBufferViews = 1;
    m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
    m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
    m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(VB);
    m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

    if (m_nIndices) {
        UINT ibSize = sizeof(UINT) * m_nIndices;
        m_pd3dIndexBuffer = CreateBufferResource(
            device, cmdList, m_pnIndices, ibSize,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

        m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
        m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
        m_d3dIndexBufferView.SizeInBytes = ibSize;
    }

    // 7) OBB
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

    // 현재는 정적 렌더
    m_bSkinnedMesh = false;

    // 로그
    std::ostringstream log;
    log << "[FBX] Mesh Loaded: " << filename << "\n"
        << "   MeshParts: " << meshes.size() << "\n"
        << "   Vertices : " << m_nVertices << "\n"
        << "   Indices  : " << m_nIndices << "\n"
        << "   Bones    : " << m_Bones.size() << "\n"
        << "   UVs      : " << (m_pxmf2TextureCoords ? "Yes" : "No") << "\n";
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

void CMesh::LoadTextureFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    ID3D12DescriptorHeap* srvHeap, UINT descriptorIndex, const wchar_t* fileName)
{
    // ============================
    // 0) Release old resources
    // ============================
    if (m_pd3dTexture) { m_pd3dTexture->Release(); m_pd3dTexture = nullptr; }
    if (m_pd3dTextureUploadBuffer) { m_pd3dTextureUploadBuffer->Release(); m_pd3dTextureUploadBuffer = nullptr; }

    // ============================
    // 1) WIC Factory 만들기
    // ============================
    IWICImagingFactory* wicFactory = nullptr;
    CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wicFactory));

    // ============================
    // 2) PNG 파일 디코더 생성
    // ============================
    IWICBitmapDecoder* decoder = nullptr;
    wicFactory->CreateDecoderFromFilename(
        fileName,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder);

    // ============================
    // 3) 첫 번째 프레임 읽기
    // ============================
    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    UINT width = 0, height = 0;
    frame->GetSize(&width, &height);

    // ============================
    // 4) RGBA 32bit 포맷으로 변환
    // ============================
    IWICFormatConverter* converter = nullptr;
    wicFactory->CreateFormatConverter(&converter);

    converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);

    // CPU 메모리 버퍼에 RGBA 데이터 복사
    UINT stride = width * 4;               // 4byte per pixel
    UINT imageSize = stride * height;
    std::unique_ptr<BYTE[]> pixels(new BYTE[imageSize]);
    converter->CopyPixels(nullptr, stride, imageSize, pixels.get());

    // ============================
    // 5) D3D12 Texture Resource 생성
    // ============================
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_pd3dTexture));

    // ============================
    // 6) 업로드 버퍼 생성
    // ============================
    UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_pd3dTexture, 0, 1);

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_pd3dTextureUploadBuffer));

    // ============================
    // 7) GPU로 텍스처 데이터 복사
    // ============================
    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = pixels.get();
    subresource.RowPitch = stride;
    subresource.SlicePitch = imageSize;

    UpdateSubresources(
        cmdList,
        m_pd3dTexture,
        m_pd3dTextureUploadBuffer,
        0, 0, 1,
        &subresource);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_pd3dTexture,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // ============================
    // 8) SRV 생성
    // ============================
    m_nTextureDescriptorIndex = descriptorIndex;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCPU(srvHeap->GetCPUDescriptorHandleForHeapStart());
    hCPU.Offset(descriptorIndex, increment);

    device->CreateShaderResourceView(m_pd3dTexture, &srvDesc, hCPU);

    // ============================
    // 정리
    // ============================
    frame->Release();
    decoder->Release();
    converter->Release();
    wicFactory->Release();
}

void CMesh::CreateSRV(ID3D12Device* device, ID3D12DescriptorHeap* srvHeap)
{
    if (!m_pd3dTexture) return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // 이 Mesh가 사용할 descriptor index
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        srvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(m_nTextureDescriptorIndex, increment);

    device->CreateShaderResourceView(
        m_pd3dTexture,
        &srvDesc,
        handle);
}
