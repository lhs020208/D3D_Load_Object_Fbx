//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Mesh.h"
#include "Animator.h"

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

/////////////////////////////////////////////////////////////////////////////////////////////////////

CMesh::CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName, int FileType)
{
	if (pstrFileName) {
		if (FileType == 1) LoadMeshFromOBJ(pd3dDevice, pd3dCommandList, pstrFileName);
		if (FileType == 2) LoadMeshFromFBX(pd3dDevice, pd3dCommandList, pstrFileName);
	}
}

CMesh::~CMesh()
{
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
	if (m_pd3dBoneIndexUploadBuffer) m_pd3dBoneIndexUploadBuffer->Release();
	if (m_pd3dBoneWeightUploadBuffer) m_pd3dBoneWeightUploadBuffer->Release();

	m_pd3dBoneIndexUploadBuffer = NULL;
	m_pd3dBoneWeightUploadBuffer = NULL;
};

void CMesh::Render(ID3D12GraphicsCommandList* cmd)
{
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& sm : m_SubMeshes)
    {
        // --------------------------------------------------------
        // 1) SubMesh 텍스처 바인딩 (textureIndex가 유효한 경우)
        // --------------------------------------------------------
        if (m_pd3dSrvDescriptorHeap && sm.textureIndex != UINT_MAX)
        {
            CD3DX12_GPU_DESCRIPTOR_HANDLE hGPU(
                m_pd3dSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
                sm.textureIndex,
                m_nSrvDescriptorIncrementSize
            );

            cmd->SetGraphicsRootDescriptorTable(
                m_nTextureRootParameterIndex, // 일반적으로 5
                hGPU
            );
        }

        // --------------------------------------------------------
        // 2) VB/IB 바인딩
        // --------------------------------------------------------
        cmd->IASetVertexBuffers(0, 1, &sm.vbView);
        cmd->IASetIndexBuffer(&sm.ibView);

        // --------------------------------------------------------
        // 3) Draw
        // --------------------------------------------------------
        cmd->DrawIndexedInstanced(
            (UINT)sm.indices.size(),
            1, 0, 0, 0
        );
    }
}


void CMesh::LoadMeshFromOBJ(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, char* filename)
{
    NULL;
}
void CMesh::LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename)
{
    // -----------------------------------------------------------------------------
    // 0) FBX 초기화
    // -----------------------------------------------------------------------------
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings())) { imp->Destroy(); mgr->Destroy(); return; }

    FbxScene* scene = FbxScene::Create(mgr, "scene");
    imp->Import(scene);
    imp->Destroy();

    // -----------------------------------------------------------------------------
    // 1) DirectX 좌표계 적용
    // -----------------------------------------------------------------------------
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // -----------------------------------------------------------------------------
    // 2) Triangulate
    // -----------------------------------------------------------------------------
    {
        FbxGeometryConverter conv(mgr);
        conv.Triangulate(scene, true);
    }

    // -----------------------------------------------------------------------------
    // 3) 모든 Mesh 수집
    // -----------------------------------------------------------------------------
    vector<FbxMesh*> meshes;
    function<void(FbxNode*)> dfs = [&](FbxNode* n)
        {
            if (!n) return;
            if (auto* m = n->GetMesh()) meshes.push_back(m);
            for (int i = 0; i < n->GetChildCount(); ++i) dfs(n->GetChild(i));
        };
    dfs(scene->GetRootNode());
    if (meshes.empty()) { mgr->Destroy(); return; }

    // -----------------------------------------------------------------------------
    // 4) 본 스켈레톤 추출 + OffsetMatrix(Inverse Bind Pose) 계산
    // -----------------------------------------------------------------------------
    m_Bones.clear();
    m_BoneNameToIndex.clear();

    // FBX Skeleton 노드 → Bone 인덱스 매핑
    std::unordered_map<FbxNode*, int> boneNodeToIndex;

    // 4-1) 스켈레톤 트리(이름, parentIndex) 수집
    std::function<void(FbxNode*, int)> GatherSkeleton = [&](FbxNode* node, int parentIndex)
        {
            if (!node) return;

            int selfIndex = parentIndex;

            if (FbxNodeAttribute* attr = node->GetNodeAttribute())
            {
                if (attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
                {
                    Bone b{};
                    b.name = node->GetName();
                    b.parentIndex = parentIndex;

                    // offsetMatrix는 일단 Identity로 초기화
                    XMStoreFloat4x4(&b.offsetMatrix, XMMatrixIdentity());

                    selfIndex = (int)m_Bones.size();
                    m_Bones.push_back(b);
                    m_BoneNameToIndex[b.name] = selfIndex;
                    boneNodeToIndex[node] = selfIndex;

                    parentIndex = selfIndex; // 자식은 이 본을 부모로 갖는다
                }
            }

            // 자식 재귀
            int childParent = parentIndex;
            for (int i = 0; i < node->GetChildCount(); ++i)
            {
                GatherSkeleton(node->GetChild(i), childParent);
            }
        };

    GatherSkeleton(scene->GetRootNode(), -1);

    // 4-2) FBX Skin/Cluster에서 실제 Inverse Bind Pose 계산
    //      offsetMatrix = LinkMatrix^-1 * MeshMatrix
    // -----------------------------------------------------------------
    // 여러 Mesh가 있을 수 있으므로, 모든 skinned mesh를 훑으며
    // 해당 Bone의 offsetMatrix를 덮어쓴다(대부분 동일 스켈레톤 기준).
    for (FbxMesh* mesh : meshes)
    {
        if (!mesh) continue;

        int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        if (skinCount <= 0) continue;

        for (int si = 0; si < skinCount; ++si)
        {
            FbxSkin* skin = (FbxSkin*)mesh->GetDeformer(si, FbxDeformer::eSkin);
            if (!skin) continue;

            int clusterCount = skin->GetClusterCount();
            for (int ci = 0; ci < clusterCount; ++ci)
            {
                FbxCluster* cluster = skin->GetCluster(ci);
                if (!cluster) continue;

                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                // Bone index 찾기
                int boneIndex = -1;

                auto itNode = boneNodeToIndex.find(linkNode);
                if (itNode != boneNodeToIndex.end())
                    boneIndex = itNode->second;
                else {
                    auto itName = m_BoneNameToIndex.find(linkNode->GetName());
                    if (itName != m_BoneNameToIndex.end())
                        boneIndex = itName->second;
                }

                if (boneIndex < 0 || boneIndex >= (int)m_Bones.size())
                    continue;

                // Mesh Bind Pose (GlobalInitialPosition)
                FbxAMatrix meshBindPose;
                cluster->GetTransformMatrix(meshBindPose);

                // Bone Bind Pose (GlobalInitialPosition)
                FbxAMatrix boneBindPose;
                cluster->GetTransformLinkMatrix(boneBindPose);

                // offset = boneBindPose^-1 * meshBindPose
                FbxAMatrix offset = boneBindPose.Inverse() * meshBindPose;

                // FBX → XM conversion
                XMFLOAT4X4 xmOffset;
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 4; ++c)
                        xmOffset.m[r][c] = (float)offset.Get(r, c);

                m_Bones[boneIndex].offsetMatrix = xmOffset;
            }
        }
    }


    // -----------------------------------------------------------------------------
    // 5) SubMesh 로 변환 (핵심)
    // -----------------------------------------------------------------------------
    m_SubMeshes.clear();

    auto ToXM3 = [&](const FbxVector4& v) { return XMFLOAT3((float)v[0], (float)v[1], (float)v[2]); };
    auto ToXM2 = [&](const FbxVector2& v) { return XMFLOAT2((float)v[0], (float)v[1]); };

    for (FbxMesh* mesh : meshes)
    {
        if (!mesh) continue;

        int polyCount = mesh->GetPolygonCount();
        int cpCount = mesh->GetControlPointsCount();
        if (!polyCount || !cpCount) continue;

        SubMesh sm;

        // =======================================================
        // Mesh 이름 저장
        // =======================================================
        if (FbxNode* node = mesh->GetNode())
        {
            sm.meshName = node->GetName();
        }
        else
        {
            sm.meshName = "UnnamedMesh";
        }

        // =======================================================
        // Material 이름 저장
        // =======================================================
        int materialCount = mesh->GetNode() ? mesh->GetNode()->GetMaterialCount() : 0;
        if (materialCount > 0)
        {
            FbxSurfaceMaterial* mat = mesh->GetNode()->GetMaterial(0);
            if (mat) sm.materialName = mat->GetName();
            else     sm.materialName = "UnnamedMaterial";
        }
        else
        {
            sm.materialName = "NoMaterial";
        }

        // ───── 트랜스폼 적용 ─────
        FbxNode* node = mesh->GetNode();
        FbxAMatrix global = node ? node->EvaluateGlobalTransform() : FbxAMatrix();

        FbxAMatrix geo;
        if (node)
        {
            geo.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
            geo.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
            geo.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));
        }
        FbxAMatrix xform = global * geo;

        bool flip = (xform.Determinant() < 0);

        // UVSet 이름
        FbxStringList uvSets;
        mesh->GetUVSetNames(uvSets);
        const char* uvSetName = (uvSets.GetCount() > 0) ? uvSets.GetStringAt(0) : nullptr;

        // ───── 정점 생성 ─────
        for (int p = 0; p < polyCount; p++)
        {
            int order[3] = { 0,1,2 };
            if (flip) std::swap(order[1], order[2]);

            for (int i = 0; i < 3; i++)
            {
                int v = order[i];
                int cpIdx = mesh->GetPolygonVertex(p, v);

                FbxVector4 cp = mesh->GetControlPointAt(cpIdx);
                FbxVector4 pw = xform.MultT(cp);

                // normal
                FbxVector4 n;
                mesh->GetPolygonVertexNormal(p, v, n);
                FbxVector4 nw = xform.MultT(FbxVector4(n[0], n[1], n[2], 0));

                double L = sqrt(nw[0] * nw[0] + nw[1] * nw[1] + nw[2] * nw[2]);
                if (L > 1e-12) { nw[0] /= L; nw[1] /= L; nw[2] /= L; }

                // uv
                XMFLOAT2 uv(0, 0);
                if (uvSetName)
                {
                    FbxVector2 u;
                    bool unm = false;
                    if (mesh->GetPolygonVertexUV(p, v, uvSetName, u, unm)) {
                        uv = ToXM2(u);
                        uv.y = 1.0f - uv.y;
                    }
                }

                sm.positions.push_back(ToXM3(pw));
                sm.normals.push_back(ToXM3(nw));
                sm.uvs.push_back(uv);

                // 스키닝 기본값
                sm.boneIndices.push_back(XMUINT4(0, 0, 0, 0));
                sm.boneWeights.push_back(XMFLOAT4(1, 0, 0, 0));

                sm.indices.push_back((UINT)sm.indices.size());
            }
        }

        m_SubMeshes.push_back(sm);
    }
    // ==========================================================================
    // 9) 각 SubMesh에 대해 BoneIndex / BoneWeight 세팅
    //    (정점은 Triangle 순서대로 push_back 된 상태)
    // ==========================================================================
    for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx)
    {
        FbxMesh* mesh = meshes[meshIdx];
        if (!mesh) continue;

        SubMesh& sm = m_SubMeshes[meshIdx];

        int vertexCount = (int)sm.positions.size();
        if (vertexCount == 0) continue;

        // 정점별 임시 저장: Bone influence 리스트
        struct Influence { int bone; float weight; };
        std::vector<std::vector<Influence>> vertexInfluences(vertexCount);

        // ----- 스킨 데이터 읽기 -----
        int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        if (skinCount <= 0) continue;

        for (int si = 0; si < skinCount; ++si)
        {
            FbxSkin* skin = (FbxSkin*)mesh->GetDeformer(si, FbxDeformer::eSkin);
            if (!skin) continue;

            int clusterCount = skin->GetClusterCount();
            for (int ci = 0; ci < clusterCount; ++ci)
            {
                FbxCluster* cluster = skin->GetCluster(ci);
                if (!cluster) continue;

                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                // bone index 찾기
                int boneIndex = -1;
                auto it = m_BoneNameToIndex.find(linkNode->GetName());
                if (it != m_BoneNameToIndex.end())
                    boneIndex = it->second;

                if (boneIndex < 0) continue;

                // cluster가 영향 주는 control point 목록
                int indexCount = cluster->GetControlPointIndicesCount();
                int* cpIndices = cluster->GetControlPointIndices();
                double* cpWeights = cluster->GetControlPointWeights();

                if (!cpIndices || !cpWeights) continue;

                // 이 FBX mesh의 정점 push-back 방식과 맞추기 위해
                // polygon -> cpIndex -> localVertexIndex 매핑 필요
                // 우리는 vertex push 순서대로 매핑 테이블을 생성할 수 있다.

                // [1] ControlPoint → SubMesh vertexIndex 매핑 테이블 생성
                //     (한 번만 만들도록 static 캐시해도 됨)
                static thread_local std::vector<std::vector<int>> cpToVertex;
                cpToVertex.clear();
                cpToVertex.resize(mesh->GetControlPointsCount());

                int polyCount = mesh->GetPolygonCount();
                int vtxCounter = 0;
                for (int p = 0; p < polyCount; ++p)
                {
                    for (int v = 0; v < 3; ++v)
                    {
                        int cpIdx = mesh->GetPolygonVertex(p, v);
                        if (cpIdx >= 0 && cpIdx < (int)cpToVertex.size())
                            cpToVertex[cpIdx].push_back(vtxCounter);
                        vtxCounter++;
                    }
                }

                // [2] cluster의 CP → 해당 SubMesh vertex에 weight 반영
                for (int k = 0; k < indexCount; ++k)
                {
                    int cpIdx = cpIndices[k];
                    float w = (float)cpWeights[k];

                    if (cpIdx < 0 || cpIdx >= (int)cpToVertex.size()) continue;

                    for (int vi : cpToVertex[cpIdx])
                    {
                        if (vi >= 0 && vi < vertexCount)
                        {
                            vertexInfluences[vi].push_back({ boneIndex, w });
                        }
                    }
                }
            }
        }

        // ----- influence 정렬 + 상위 4개만 남기기 + 정규화 -----
        for (int i = 0; i < vertexCount; ++i)
        {
            auto& inf = vertexInfluences[i];

            if (inf.empty())
            {
                // 영향이 없으면 bone0 = weight1
                sm.boneIndices[i] = XMUINT4(0, 0, 0, 0);
                sm.boneWeights[i] = XMFLOAT4(1, 0, 0, 0);
                continue;
            }

            // weight 큰 순 정렬
            std::sort(inf.begin(), inf.end(), [](const Influence& a, const Influence& b) {
                return a.weight > b.weight;
                });

            // 최대 4개만 사용
            if (inf.size() > 4) inf.resize(4);

            // 정규화
            float total = 0;
            for (auto& x : inf) total += x.weight;
            if (total < 1e-8f) total = 1.0f;
            float inv = 1.0f / total;

            // sm.boneIndices / sm.boneWeights 채우기
            XMUINT4 idx(0, 0, 0, 0);
            XMFLOAT4 w(0, 0, 0, 0);

            for (size_t k = 0; k < inf.size(); ++k)
            {
                (&idx.x)[k] = inf[k].bone;
                (&w.x)[k] = inf[k].weight * inv;
            }

            sm.boneIndices[i] = idx;
            sm.boneWeights[i] = w;
        }
    }

    // -----------------------------------------------------------------------------
    // 6) 원래 OBB 계산 기능 유지
    // -----------------------------------------------------------------------------
    if (!m_SubMeshes.empty())
    {
        XMFLOAT3 mn(1e9, 1e9, 1e9), mx(-1e9, -1e9, -1e9);

        for (auto& sm : m_SubMeshes)
        {
            for (auto& p : sm.positions)
            {
                mn.x = min(mn.x, p.x);
                mn.y = min(mn.y, p.y);
                mn.z = min(mn.z, p.z);

                mx.x = max(mx.x, p.x);
                mx.y = max(mx.y, p.y);
                mx.z = max(mx.z, p.z);
            }
        }

        XMFLOAT3 c{ (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
        XMFLOAT3 e{ (mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f, (mx.z - mn.z) * 0.5f };
        m_xmOOBB = BoundingOrientedBox(c, e, XMFLOAT4(0, 0, 0, 1));
    }

    if (m_pAnimator)
    {
        int nBones = (int)m_Bones.size();

        // 1) 본 개수 전달
        m_pAnimator->SetBoneCount(nBones);

        // 2) OffsetMatrix / ParentIndex 전달
        for (int i = 0; i < nBones; ++i)
        {
            m_pAnimator->SetBoneOffsetMatrix(i, m_Bones[i].offsetMatrix);
            m_pAnimator->SetBoneParent(i, m_Bones[i].parentIndex);
        }
    }

    // -----------------------------------------------------------------------------
    // 7) 정적 메쉬 (스키닝 끔)
    // -----------------------------------------------------------------------------
    m_bSkinnedMesh = false;

    // -----------------------------------------------------------------------------
    // 8) SubMesh GPU VB/IB 생성
    // -----------------------------------------------------------------------------
    for (auto& sm : m_SubMeshes)
    {
        // --- Vertex Buffer (pos+normal+uv 하나로 묶어 업로드) ---
        struct VTX { XMFLOAT3 pos; XMFLOAT3 n; XMFLOAT2 uv; };

        vector<VTX> vtx(sm.positions.size());
        for (size_t i = 0; i < vtx.size(); i++) {
            vtx[i].pos = sm.positions[i];
            vtx[i].n = sm.normals[i];
            vtx[i].uv = sm.uvs[i];
        }

        UINT vbSize = (UINT)(sizeof(VTX) * vtx.size());

        sm.vb = CreateBufferResource(
            device, cmdList,
            vtx.data(), vbSize,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            &sm.vbUpload
        );

        sm.vbView.BufferLocation = sm.vb->GetGPUVirtualAddress();
        sm.vbView.SizeInBytes = vbSize;
        sm.vbView.StrideInBytes = sizeof(VTX);

        // --- Index Buffer ---
        UINT ibSize = (UINT)(sizeof(UINT) * sm.indices.size());

        sm.ib = CreateBufferResource(
            device, cmdList,
            sm.indices.data(), ibSize,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            &sm.ibUpload
        );

        sm.ibView.BufferLocation = sm.ib->GetGPUVirtualAddress();
        sm.ibView.SizeInBytes = ibSize;
        sm.ibView.Format = DXGI_FORMAT_R32_UINT;
    }


    // 로그
    std::ostringstream log;
    log << "[FBX] Mesh Loaded: " << filename << "\n"
        << "   SubMeshes: " << m_SubMeshes.size() << "\n"
        << "   Bones    : " << m_Bones.size() << "\n";
    OutputDebugStringA(log.str().c_str());

    mgr->Destroy();
}

void CMesh::EnableSkinning(ID3D12Device* device,  ID3D12GraphicsCommandList* cmdList, int nBones)
{
    // 스키닝 활성화
    m_bSkinnedMesh = true;

    // ===============================
    // 1) CPU 배열 할당
    // ===============================
    // 기존 것이 있으면 해제
    if (m_pxmf4x4BoneTransforms)
    {
        delete[] m_pxmf4x4BoneTransforms;
        m_pxmf4x4BoneTransforms = nullptr;
    }

    m_pxmf4x4BoneTransforms = new XMFLOAT4X4[nBones];
    for (int i = 0; i < nBones; ++i)
        XMStoreFloat4x4(&m_pxmf4x4BoneTransforms[i], XMMatrixIdentity());

    // ===============================
    // 2) GPU CBV (b4) 생성
    // ===============================
    if (m_pd3dcbBoneTransforms)
    {
        m_pd3dcbBoneTransforms->Release();
        m_pd3dcbBoneTransforms = nullptr;
    }

    UINT bufferSize = sizeof(XMFLOAT4X4) * nBones;

    // 상수 버퍼는 256byte alignment 필요
    UINT alignedSize = (bufferSize + 255) & ~255;

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(alignedSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_pd3dcbBoneTransforms)
    );

    // 첫 프레임용 초기 업로드
    {
        void* mapped = nullptr;
        m_pd3dcbBoneTransforms->Map(0, nullptr, &mapped);

        memcpy(mapped, m_pxmf4x4BoneTransforms, bufferSize);

        m_pd3dcbBoneTransforms->Unmap(0, nullptr);
    }
}



void CMesh::SetPolygon(int nIndex, CPolygon* pPolygon)
{
	if ((0 <= nIndex) && (nIndex < m_nPolygons)) m_ppPolygons[nIndex] = pPolygon;
}

int CMesh::CheckRayIntersection(XMVECTOR& rayOrigin, XMVECTOR& rayDir, float* pfNearHitDistance)
{
    int hitCount = 0;
    float nearest = FLT_MAX;

    for (auto& sm : m_SubMeshes)
    {
        const auto& pos = sm.positions;
        const auto& idx = sm.indices;

        for (size_t i = 0; i < idx.size(); i += 3)
        {
            XMVECTOR v0 = XMLoadFloat3(&pos[idx[i]]);
            XMVECTOR v1 = XMLoadFloat3(&pos[idx[i + 1]]);
            XMVECTOR v2 = XMLoadFloat3(&pos[idx[i + 2]]);

            float dist = 0.0f;
            if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, dist))
            {
                if (dist < nearest)
                {
                    nearest = dist;
                    hitCount++;
                    if (pfNearHitDistance) *pfNearHitDistance = nearest;
                }
            }
        }
    }

    return hitCount;
}

BOOL CMesh::RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance)
{
	float fHitDistance;
	BOOL bIntersected = TriangleTests::Intersects(xmRayOrigin, xmRayDirection, v0, v1, v2, fHitDistance);
	if (bIntersected && (fHitDistance < *pfNearHitDistance)) *pfNearHitDistance = fHitDistance;

	return(bIntersected);
}

void CMesh::LoadTextureFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
    ID3D12DescriptorHeap* srvHeap, UINT descriptorIndex, const wchar_t* fileName, int subMeshIndex)
{
    if (subMeshIndex < 0 || subMeshIndex >= (int)m_SubMeshes.size())
        return;

    SubMesh& sm = m_SubMeshes[subMeshIndex];

    // ---- 기존 텍스처 해제 (SubMesh 전용) ----
    if (sm.texture) { sm.texture->Release(); sm.texture = nullptr; }
    if (sm.textureUpload) { sm.textureUpload->Release(); sm.textureUpload = nullptr; }

    // ---- 1) WIC 로딩 ----
    IWICImagingFactory* wicFactory = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

    IWICBitmapDecoder* decoder = nullptr;
    wicFactory->CreateDecoderFromFilename(
        fileName, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);

    if (!decoder) return;

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);

    UINT width = 0, height = 0;
    frame->GetSize(&width, &height);

    IWICFormatConverter* converter = nullptr;
    wicFactory->CreateFormatConverter(&converter);

    converter->Initialize(
        frame,
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);

    UINT stride = width * 4;
    UINT imageSize = stride * height;
    std::unique_ptr<BYTE[]> pixels(new BYTE[imageSize]);
    converter->CopyPixels(0, stride, imageSize, pixels.get());

    // ---- 2) GPU 텍스처 생성 (SubMesh 전용) ----
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&sm.texture));

    UINT64 uploadSize = GetRequiredIntermediateSize(sm.texture, 0, 1);

    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&sm.textureUpload));

    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData = pixels.get();
    sub.RowPitch = stride;
    sub.SlicePitch = imageSize;

    UpdateSubresources(cmdList, sm.texture, sm.textureUpload,
        0, 0, 1, &sub);

    cmdList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(
            sm.texture,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    // ---- 3) SRV 생성 ----
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    UINT inc = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE hCPU(
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        descriptorIndex, inc);

    device->CreateShaderResourceView(sm.texture, &srvDesc, hCPU);

    // ---- 4) 이 SubMesh가 사용할 SRV 인덱스 저장 ----
    sm.textureIndex = descriptorIndex;

    frame->Release();
    decoder->Release();
    converter->Release();
    wicFactory->Release();
}

void CMesh::SetSrvDescriptorInfo(ID3D12DescriptorHeap* heap, UINT inc)
{
    m_pd3dSrvDescriptorHeap = heap;
    m_nSrvDescriptorIncrementSize = inc;
}

void CMesh::SetAnimator(CAnimator* pAnimator)
{
    m_pAnimator = pAnimator;
}

void CMesh::UpdateBoneTransforms(ID3D12GraphicsCommandList* cmd)
{
    if (!m_pAnimator || !m_bSkinnedMesh) return;

    const auto& finalMats = m_pAnimator->GetFinalBoneMatrices();
    int boneCount = (int)finalMats.size();

    // CPU-side m_pxmf4x4BoneTransforms 에 복사
    for (int i = 0; i < boneCount; ++i)
        m_pxmf4x4BoneTransforms[i] = finalMats[i];

    // GPU CBV에 업로드
    void* mapped = nullptr;
    m_pd3dcbBoneTransforms->Map(0, nullptr, &mapped);
    memcpy(mapped, m_pxmf4x4BoneTransforms, sizeof(XMFLOAT4X4) * boneCount);
    m_pd3dcbBoneTransforms->Unmap(0, nullptr);

    // RootSignature slot 4 → BoneMatrix CBV 바인딩
    cmd->SetGraphicsRootConstantBufferView(4,
        m_pd3dcbBoneTransforms->GetGPUVirtualAddress());
}

void CMesh::LoadAnimationFromFBX(const char* filename)
{
    if (!m_pAnimator)
        return; // Animator가 없으면 애니메이션 저장 불가

    // =============================
    // 0) FBX 초기화
    // =============================
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* importer = FbxImporter::Create(mgr, "");
    if (!importer->Initialize(filename, -1, mgr->GetIOSettings()))
    {
        importer->Destroy();
        mgr->Destroy();
        return;
    }

    FbxScene* scene = FbxScene::Create(mgr, "animScene");
    importer->Import(scene);
    importer->Destroy();

    // =============================
    // 1) DirectX 좌표계 적용
    // =============================
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // =============================
    // 2) AnimStack 얻기
    // =============================
    int animStackCount = scene->GetSrcObjectCount<FbxAnimStack>();
    if (animStackCount <= 0)
    {
        mgr->Destroy();
        return;
    }

    // 보통 하나의 FBX에는 하나의 Stack이 있다.
    FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(0);
    scene->SetCurrentAnimationStack(animStack);

    const char* clipName = animStack->GetName();

    // =============================
    // 3) 첫 번째 AnimLayer 사용
    // =============================
    int layerCount = animStack->GetMemberCount<FbxAnimLayer>();
    if (layerCount <= 0)
    {
        mgr->Destroy();
        return;
    }
    FbxAnimLayer* layer = animStack->GetMember<FbxAnimLayer>(0);

    // =============================
    // 4) AnimationClip 생성
    // =============================
    AnimationClip clip;
    clip.name = clipName;

    // animStack 기간 계산 (초 단위)
    FbxTakeInfo* takeInfo = scene->GetTakeInfo(clipName);
    if (takeInfo)
    {
        FbxTime span = takeInfo->mLocalTimeSpan.GetDuration();
        clip.duration = (float)span.GetSecondDouble();
    }

    // =============================
    // 5) 각 Bone 노드를 순회하면서 Track 생성
    // =============================
    for (size_t boneIdx = 0; boneIdx < m_Bones.size(); ++boneIdx)
    {
        const Bone& bone = m_Bones[boneIdx];
        const char* boneName = bone.name.c_str();

        // FBX Scene에서 해당 이름의 노드 찾기
        FbxNode* boneNode = scene->FindNodeByName(boneName);
        if (!boneNode) continue;

        BoneKeyframes track;
        track.boneName = boneName;
        track.boneIndex = (int)boneIdx;

        // Position / Rotation / Scale CurveNode 얻기
        FbxAnimCurve* tCurve[3] = {
            boneNode->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X),
            boneNode->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y),
            boneNode->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z)
        };

        FbxAnimCurve* rCurve[3] = {
            boneNode->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X),
            boneNode->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y),
            boneNode->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z)
        };

        FbxAnimCurve* sCurve[3] = {
            boneNode->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X),
            boneNode->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y),
            boneNode->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z)
        };

        // 키 개수 구하기 (T/R/S 중 가장 많은 keyCount 사용)
        int keyCount = 0;
        for (int i = 0; i < 3; ++i)
        {
            if (tCurve[i]) keyCount = max(keyCount, tCurve[i]->KeyGetCount());
            if (rCurve[i]) keyCount = max(keyCount, rCurve[i]->KeyGetCount());
            if (sCurve[i]) keyCount = max(keyCount, sCurve[i]->KeyGetCount());
        }

        // 해당 본에 키프레임이 없는 경우 → 트랙 추가하지 않음
        if (keyCount == 0) continue;

        // =============================
        // 6) Keyframe 생성
        // =============================
        for (int k = 0; k < keyCount; ++k)
        {
            AnimationKey key{};

            // 시간 (기준: T curve 사용, 없으면 R or S)
            FbxTime t;
            if (tCurve[0] && k < tCurve[0]->KeyGetCount())
                t = tCurve[0]->KeyGetTime(k);
            else if (tCurve[1] && k < tCurve[1]->KeyGetCount())
                t = tCurve[1]->KeyGetTime(k);
            else if (tCurve[2] && k < tCurve[2]->KeyGetCount())
                t = tCurve[2]->KeyGetTime(k);
            else if (rCurve[0] && k < rCurve[0]->KeyGetCount())
                t = rCurve[0]->KeyGetTime(k);
            else if (sCurve[0] && k < sCurve[0]->KeyGetCount())
                t = sCurve[0]->KeyGetTime(k);

            key.time = (float)t.GetSecondDouble();

            // --- Translation ---
            key.translation.x = (tCurve[0] && k < tCurve[0]->KeyGetCount()) ? (float)tCurve[0]->KeyGetValue(k) : 0.0f;
            key.translation.y = (tCurve[1] && k < tCurve[1]->KeyGetCount()) ? (float)tCurve[1]->KeyGetValue(k) : 0.0f;
            key.translation.z = (tCurve[2] && k < tCurve[2]->KeyGetCount()) ? (float)tCurve[2]->KeyGetValue(k) : 0.0f;

            // --- Rotation (Euler → Quaternion) ---
            float rx = (rCurve[0] && k < rCurve[0]->KeyGetCount()) ? (float)rCurve[0]->KeyGetValue(k) : 0.0f;
            float ry = (rCurve[1] && k < rCurve[1]->KeyGetCount()) ? (float)rCurve[1]->KeyGetValue(k) : 0.0f;
            float rz = (rCurve[2] && k < rCurve[2]->KeyGetCount()) ? (float)rCurve[2]->KeyGetValue(k) : 0.0f;

            // FBX Rotation은 Degrees
            XMVECTOR q = XMQuaternionRotationRollPitchYaw(
                XMConvertToRadians(rx),
                XMConvertToRadians(ry),
                XMConvertToRadians(rz)
            );
            XMStoreFloat4(&key.rotation, q);

            // --- Scale ---
            key.scale.x = (sCurve[0] && k < sCurve[0]->KeyGetCount()) ? (float)sCurve[0]->KeyGetValue(k) : 1.0f;
            key.scale.y = (sCurve[1] && k < sCurve[1]->KeyGetCount()) ? (float)sCurve[1]->KeyGetValue(k) : 1.0f;
            key.scale.z = (sCurve[2] && k < sCurve[2]->KeyGetCount()) ? (float)sCurve[2]->KeyGetValue(k) : 1.0f;

            track.keys.push_back(key);
        }

        clip.boneTracks.push_back(track);
    }

    // =============================
    // 7) Animator에 Clip 등록
    // =============================
    m_pAnimator->AddClip(clip);

    // =============================
    // Fbx Cleanup
    // =============================
    mgr->Destroy();
}
