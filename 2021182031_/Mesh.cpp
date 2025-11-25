//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Mesh.h"
#include "Animator.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{
    // 한 노드(=본)에 대한 모든 키 타임을 모은다.
    void CollectKeyTimes(FbxNode* node, FbxAnimLayer* layer, std::set<FbxTime>& outTimes)
    {
        auto addCurve = [&](FbxAnimCurve* curve)
            {
                if (!curve) return;
                int keyCount = curve->KeyGetCount();
                for (int i = 0; i < keyCount; ++i)
                {
                    outTimes.insert(curve->KeyGetTime(i));
                }
            };

        addCurve(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
        addCurve(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
        addCurve(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));

        addCurve(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
        addCurve(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
        addCurve(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));

        addCurve(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
        addCurve(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
        addCurve(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));
    }

    // 하나의 본 노드에 대해 BoneKeyframes 를 채운다.
    // ============================================================================
    // ExtractBoneTrack (REVISED)
    //   - FBX 로컬 TRS만 추출
    //   - bindLocal 관련 보정 삭제
    //   - corrected = bindInv * anim * bind 구문은 보존만 함(사용 X)
    // ============================================================================
    void ExtractBoneTrack(
        FbxNode* node,
        int boneIndex,
        const std::vector<Bone>& bones,
        FbxAnimLayer* layer,
        const FbxTimeSpan& timeSpan,
        float timeScale,
        AnimationClip& clip)
    {
        if (!node || boneIndex < 0) return;

        // --- 키 시간 수집 ---
        std::set<FbxTime> keyTimes;
        CollectKeyTimes(node, layer, keyTimes);

        if (keyTimes.empty())
            return;

        BoneKeyframes& track = clip.boneTracks[boneIndex];

        const double startSec = timeSpan.GetStart().GetSecondDouble();

        // ------------------------------------------------------------------------
        // OLD: bindLocal 보정용으로 bind/bindInv를 사용했으나
        //      NEW 방식에서는 사용하지 않는다.
        // ------------------------------------------------------------------------
        /*
        const Bone& bone = bones[boneIndex];
        XMMATRIX bind    = XMLoadFloat4x4(&bone.bindLocal);
        XMMATRIX bindInv = XMMatrixInverse(nullptr, bind);
        */

        // ------------------------------------------------------------------------
        // NEW 방식: FBX 로컬 TRS만 사용
        // ------------------------------------------------------------------------
        for (const FbxTime& t : keyTimes)
        {
            if (t < timeSpan.GetStart() || t > timeSpan.GetStop())
                continue;

            // ----- FBX 로컬 행렬 -----
            FbxAMatrix fbxLocal = node->EvaluateLocalTransform(t);

            // ----- TRS 분해 -----
            FbxVector4 T = fbxLocal.GetT();
            FbxQuaternion R = fbxLocal.GetQ();
            FbxVector4 S = fbxLocal.GetS();

            Keyframe k;
            k.timeSec = (float)((t.GetSecondDouble() - startSec) * timeScale);

            k.translation = XMFLOAT3(
                (float)T[0],
                (float)T[1],
                (float)T[2]
            );

            k.rotationQuat = XMFLOAT4(
                (float)R[0],
                (float)R[1],
                (float)R[2],
                (float)R[3]
            );

            k.scale = XMFLOAT3(
                (float)S[0],
                (float)S[1],
                (float)S[2]
            );

            track.keyframes.push_back(k);

            // --------------------------------------------------------------------
            // OLD LOGIC (사용 X, 보존만 함)
            // corrected = bindInv * anim * bind
            // --------------------------------------------------------------------
            /*
            XMMATRIX anim = XMLoadFloat4x4(&animF);
            XMMATRIX corrected = bindInv * anim * bind;

            XMVECTOR S2, R2, T2;
            XMMatrixDecompose(&S2, &R2, &T2, corrected);

            Keyframe k_old;
            k_old.timeSec = k.timeSec;
            XMStoreFloat3(&k_old.translation, T2);
            XMStoreFloat4(&k_old.rotationQuat, R2);
            k_old.scale = XMFLOAT3(1,1,1);
            */
        }

        // 시간 순 정렬
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const Keyframe& a, const Keyframe& b)
            {
                return a.timeSec < b.timeSec;
            });
    }




    // 씬 트리 전체를 돌며 본 이름과 일치하는 노드에서 트랙을 뽑는다.
    void TraverseAndExtractTracks(
        FbxNode* node,
        FbxAnimLayer* layer,
        const std::vector<Bone>& bones,                       // 추가
        const std::unordered_map<std::string, int>& boneNameToIndex,
        const FbxTimeSpan& timeSpan,
        float timeScale,
        AnimationClip& clip)
    {
        if (!node) return;

        const char* nodeNameC = node->GetName();
        std::string nodeName = nodeNameC ? nodeNameC : "";

        auto it = boneNameToIndex.find(nodeName);
        if (it != boneNameToIndex.end())
        {
            int boneIndex = it->second;
            ExtractBoneTrack(node, boneIndex, bones,           // bones 전달
                layer, timeSpan, timeScale, clip);
        }

        int childCount = node->GetChildCount();
        for (int i = 0; i < childCount; ++i)
        {
            TraverseAndExtractTracks(node->GetChild(i), layer,
                bones,                                         // 전달
                boneNameToIndex, timeSpan, timeScale, clip);
        }
    }

} // anonymous namespace

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
    m_pd3dDevice = pd3dDevice;
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
}
// ===============================================================================================
//  LoadMeshFromFBX (FINAL VERSION)
//  - 자동 base-mesh 선택
//  - bind pose alignment 적용
//  - non-skinned mesh 자동 정렬
//  - skin mesh vertex 좌표계를 bind pose 기준으로 통일
// ===============================================================================================
void CMesh::LoadMeshFromFBX(ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const char* filename)
{
    // -------------------------------------------------------------------------
    // 0) FBX 초기화
    // -------------------------------------------------------------------------
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings())) {
        imp->Destroy();
        mgr->Destroy();
        return;
    }

    FbxScene* scene = FbxScene::Create(mgr, "scene");
    imp->Import(scene);
    imp->Destroy();

    // -------------------------------------------------------------------------
    // 1) DirectX 좌표계 적용
    // -------------------------------------------------------------------------
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // -------------------------------------------------------------------------
    // 2) Triangulate
    // -------------------------------------------------------------------------
    {
        FbxGeometryConverter conv(mgr);
        conv.Triangulate(scene, true);
    }

    // -------------------------------------------------------------------------
    // 3) 모든 Mesh 수집 + 스킨 여부 파악
    // -------------------------------------------------------------------------
    std::vector<FbxMesh*> meshes;
    std::vector<bool>     meshHasSkin;
    std::vector<int>      meshVertexCount;

    std::function<void(FbxNode*)> dfs = [&](FbxNode* n)
        {
            if (!n) return;
            if (auto* m = n->GetMesh())
            {
                meshes.push_back(m);
                meshHasSkin.push_back(m->GetDeformerCount(FbxDeformer::eSkin) > 0);
                meshVertexCount.push_back(m->GetControlPointsCount());
            }
            for (int i = 0; i < n->GetChildCount(); ++i)
                dfs(n->GetChild(i));
        };
    dfs(scene->GetRootNode());

    if (meshes.empty()) {
        mgr->Destroy();
        return;
    }

    // -------------------------------------------------------------------------
    // 4) Bone skeleton 수집
    // -------------------------------------------------------------------------
    m_Bones.clear();
    m_BoneNameToIndex.clear();

    std::function<void(FbxNode*, int)> ExtractBones = [&](FbxNode* node, int parentIdx)
        {
            if (!node) return;
            FbxNodeAttribute* attr = node->GetNodeAttribute();

            int myIdx = parentIdx;
            if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
            {
                Bone b{};
                b.name = node->GetName();
                b.parentIndex = parentIdx;
                XMStoreFloat4x4(&b.bindLocal, XMMatrixIdentity());
                XMStoreFloat4x4(&b.offsetMatrix, XMMatrixIdentity());
                XMStoreFloat4x4(&b.animRestLocal, XMMatrixIdentity());
                XMStoreFloat4x4(&b.deltaLocal, XMMatrixIdentity());

                myIdx = (int)m_Bones.size();
                m_BoneNameToIndex[b.name] = myIdx;
                m_Bones.push_back(b);
            }
            for (int i = 0; i < node->GetChildCount(); ++i)
                ExtractBones(node->GetChild(i), myIdx);
        };
    ExtractBones(scene->GetRootNode(), -1);

    const int boneCount = (int)m_Bones.size();

    // -------------------------------------------------------------------------
    // 5) base-mesh 자동 선택 (가장 큰 vertex count를 가진 skinned mesh)
    // -------------------------------------------------------------------------
    int baseMeshIndex = -1;
    int maxVerts = -1;

    for (int i = 0; i < (int)meshes.size(); ++i)
    {
        if (!meshHasSkin[i]) continue;
        if (meshVertexCount[i] > maxVerts)
        {
            maxVerts = meshVertexCount[i];
            baseMeshIndex = i;
        }
    }

    if (baseMeshIndex < 0)
    {
        // 스키닝 없는 모델: 임의로 0번을 base mesh로 사용
        baseMeshIndex = 0;
    }

    FbxMesh* baseMesh = meshes[baseMeshIndex];
    FbxNode* baseNode = baseMesh->GetNode();

    // -------------------------------------------------------------------------
    // 6) boneGlobalBind: "메시 로컬 공간" 기준 본의 바인드 포즈 계산
    //      - cluster 행렬은 쓰지 않고, 노드의 글로벌 행렬만 사용
    //      - boneGlobalBind[i] = (baseMeshGlobal^-1) * boneGlobal
    // -------------------------------------------------------------------------
    std::vector<FbxAMatrix> boneGlobalBind(boneCount);
    std::vector<bool>       boneHasBind(boneCount, false);

    FbxAMatrix baseMeshGlobal;
    if (baseNode)
        baseMeshGlobal = baseNode->EvaluateGlobalTransform();
    else
        baseMeshGlobal.SetIdentity();

    FbxAMatrix baseMeshGlobalInv = baseMeshGlobal.Inverse();

    for (int i = 0; i < boneCount; ++i)
    {
        FbxNode* boneNode = scene->FindNodeByName(m_Bones[i].name.c_str());
        if (!boneNode)
        {
            boneGlobalBind[i].SetIdentity();
            continue;
        }

        // 본의 글로벌(씬 기준) → 메시 로컬 기준으로 변환
        FbxAMatrix boneGlobal = boneNode->EvaluateGlobalTransform();
        FbxAMatrix boneInMesh = baseMeshGlobalInv * boneGlobal; // ← 메시 로컬 기준

        boneGlobalBind[i] = boneInMesh;
        boneHasBind[i] = true;
    }

    // -------------------------------------------------------------------------
    // 7) bindLocal 계산 (부모 기준 로컬 bind pose)
    // -------------------------------------------------------------------------
    for (int i = 0; i < boneCount; ++i)
    {
        if (!boneHasBind[i])
        {
            FbxAMatrix I; I.SetIdentity();
            boneGlobalBind[i] = I;
            boneHasBind[i] = true;
        }

        int p = m_Bones[i].parentIndex;

        FbxAMatrix parentM;
        if (p >= 0 && boneHasBind[p])
            parentM = boneGlobalBind[p];
        else
            parentM.SetIdentity();

        FbxAMatrix local = parentM.Inverse() * boneGlobalBind[i];

        XMFLOAT4X4 xm{};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                xm.m[r][c] = (float)local.Get(r, c);

        m_Bones[i].bindLocal = xm;
    }

    // -------------------------------------------------------------------------
    // 8) offsetMatrix = inverse(boneGlobalBind)
    //     - 모델(메시 로컬) 공간 → 본 공간
    // -------------------------------------------------------------------------
    for (int i = 0; i < boneCount; ++i)
    {
        FbxAMatrix off = boneGlobalBind[i].Inverse();

        XMFLOAT4X4 xm{};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                xm.m[r][c] = (float)off.Get(r, c);

        m_Bones[i].offsetMatrix = xm;
    }

    // -------------------------------------------------------------------------
    // 9) SubMesh 생성 (UV 복원 추가됨)
    // -------------------------------------------------------------------------
    m_SubMeshes.clear();

    auto ToXM3 = [&](const FbxVector4& v) { return XMFLOAT3((float)v[0], (float)v[1], (float)v[2]); };
    auto ToXM2 = [&](const FbxVector2& v) { return XMFLOAT2((float)v[0], (float)v[1]); };

    for (int mi = 0; mi < (int)meshes.size(); ++mi)
    {
        FbxMesh* mesh = meshes[mi];
        if (!mesh) continue;

        SubMesh sm;

        // 이름
        FbxNode* node = mesh->GetNode();
        sm.meshName = node ? node->GetName() : "Unnamed";
        sm.materialName = "";
        int matCount = node->GetMaterialCount();
        if (matCount > 0)
        {
            FbxSurfaceMaterial* mat = node->GetMaterial(0);
            if (mat)
            {
                sm.materialName = mat->GetName();
            }
        }
        // triangle winding flip 감지
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

        // non-skinned mesh라면, 가장 가까운 Skeleton 노드를 찾아 그 본에 붙인다.
        int attachedBoneIndex = -1;
        if (!meshHasSkin[mi] && !m_Bones.empty())
        {
            FbxNode* cur = node;
            while (cur)
            {
                auto itBone = m_BoneNameToIndex.find(cur->GetName());
                if (itBone != m_BoneNameToIndex.end())
                {
                    attachedBoneIndex = itBone->second;
                    break;
                }
                cur = cur->GetParent();
            }
            if (attachedBoneIndex < 0)
                attachedBoneIndex = 0; // 못 찾으면 root 본에 붙인다.
        }

        // 정점 변환
        int polyCount = mesh->GetPolygonCount();
        int cpCount = mesh->GetControlPointsCount();
        FbxVector4* cp = mesh->GetControlPoints();

        // --- UV 세트 이름 얻기 (첫 번째 세트 사용) ---
        FbxStringList uvSetNames;
        mesh->GetUVSetNames(uvSetNames);
        const char* uvSetName = nullptr;
        if (uvSetNames.GetCount() > 0)
            uvSetName = uvSetNames[0];
        const bool hasUVSet = (uvSetName != nullptr);

        for (int p = 0; p < polyCount; ++p)
        {
            int idx[3] = { 0,1,2 };
            if (flip) std::swap(idx[1], idx[2]);

            for (int k = 0; k < 3; ++k)
            {
                int cpIdx = mesh->GetPolygonVertex(p, idx[k]);
                if (cpIdx < 0 || cpIdx >= cpCount) continue;

                FbxVector4 pos = cp[cpIdx];
                sm.positions.push_back(ToXM3(pos));

                FbxVector4 n;
                mesh->GetPolygonVertexNormal(p, idx[k], n);
                sm.normals.push_back(ToXM3(n));

                // -----------------------------
                // UV 복원
                // -----------------------------
                if (hasUVSet)
                {
                    FbxVector2 uv;
                    bool unmapped = false;
                    if (mesh->GetPolygonVertexUV(p, idx[k], uvSetName, uv, unmapped))
                    {
                        if (mesh->GetPolygonVertexUV(p, idx[k], uvSetName, uv, unmapped))
                        {
                            sm.uvs.push_back(XMFLOAT2((float)uv[0], 1.0f - (float)uv[1]));
                        }

                    }
                    else
                    {
                        sm.uvs.push_back(XMFLOAT2(0, 0));
                    }
                }
                else
                {
                    sm.uvs.push_back(XMFLOAT2(0, 0));
                }

                sm.indices.push_back((UINT)sm.indices.size());

                // non-skinned mesh → attachedBoneIndex에 weight=1
                if (!meshHasSkin[mi])
                {
                    XMUINT4  bi(0, 0, 0, 0);
                    XMFLOAT4 bw(0, 0, 0, 0);

                    if (attachedBoneIndex >= 0)
                    {
                        bi.x = (UINT)attachedBoneIndex;
                        bw.x = 1.0f;
                    }

                    sm.boneIndices.push_back(bi);
                    sm.boneWeights.push_back(bw);
                }
            }
        }

        if (meshHasSkin[mi])
        {
            FillSkinWeights(mesh, sm);
        }
        else
        {
            if (sm.boneIndices.size() != sm.positions.size())
            {
                sm.boneIndices.resize(sm.positions.size(), XMUINT4(0, 0, 0, 0));
                sm.boneWeights.resize(sm.positions.size(), XMFLOAT4(0, 0, 0, 0));

                if (attachedBoneIndex >= 0)
                {
                    for (size_t i = 0; i < sm.positions.size(); ++i)
                    {
                        sm.boneIndices[i].x = (UINT)attachedBoneIndex;
                        sm.boneWeights[i].x = 1.0f;
                    }
                }
            }
        }

        m_SubMeshes.push_back(sm);
    }


    // -------------------------------------------------------------------------
    // 10) GPU VB/IB 생성 (기존 동일)
    // -------------------------------------------------------------------------
    for (auto& sm : m_SubMeshes)
    {
        const auto& positions = sm.positions;
        const auto& normals = sm.normals;
        const auto& uvs = sm.uvs;
        const auto& indices = sm.indices;
        const auto& boneIndices = sm.boneIndices;
        const auto& boneWeights = sm.boneWeights;

        std::vector<SkinnedVertex> vertices(positions.size());

        for (size_t i = 0; i < positions.size(); ++i)
        {
            SkinnedVertex v{};
            v.position = positions[i];
            v.normal = (i < normals.size() ? normals[i] : XMFLOAT3(0, 1, 0));
            v.uv = (i < uvs.size() ? uvs[i] : XMFLOAT2(0, 0));

            if (i < boneIndices.size())
            {
                const XMUINT4& bi = boneIndices[i];
                v.boneIndices[0] = bi.x;
                v.boneIndices[1] = bi.y;
                v.boneIndices[2] = bi.z;
                v.boneIndices[3] = bi.w;
            }
            else
            {
                v.boneIndices[0] = 0;
                v.boneIndices[1] = 0;
                v.boneIndices[2] = 0;
                v.boneIndices[3] = 0;
            }

            if (i < boneWeights.size())
            {
                const XMFLOAT4& bw = boneWeights[i];
                v.boneWeights[0] = bw.x;
                v.boneWeights[1] = bw.y;
                v.boneWeights[2] = bw.z;
                v.boneWeights[3] = bw.w;
            }
            else
            {
                v.boneWeights[0] = 1.0f;
                v.boneWeights[1] = 0.0f;
                v.boneWeights[2] = 0.0f;
                v.boneWeights[3] = 0.0f;
            }

            vertices[i] = v;
        }

        UINT vbSize = sizeof(SkinnedVertex) * (UINT)vertices.size();

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

        HRESULT hr = device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&sm.vb));

        CD3DX12_HEAP_PROPERTIES uploadProps(D3D12_HEAP_TYPE_UPLOAD);
        hr = device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&sm.vbUpload));

        void* mapped = nullptr;
        CD3DX12_RANGE range(0, 0);
        sm.vbUpload->Map(0, &range, &mapped);
        memcpy(mapped, vertices.data(), vbSize);
        sm.vbUpload->Unmap(0, nullptr);

        cmdList->CopyBufferRegion(sm.vb, 0, sm.vbUpload, 0, vbSize);

        CD3DX12_RESOURCE_BARRIER vbBarrier =
            CD3DX12_RESOURCE_BARRIER::Transition(
                sm.vb,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        cmdList->ResourceBarrier(1, &vbBarrier);

        sm.vbView.BufferLocation = sm.vb->GetGPUVirtualAddress();
        sm.vbView.SizeInBytes = vbSize;
        sm.vbView.StrideInBytes = sizeof(SkinnedVertex);

        if (!indices.empty())
        {
            UINT ibSize = sizeof(uint32_t) * (UINT)indices.size();

            CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

            hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &ibDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&sm.ib));

            hr = device->CreateCommittedResource(
                &uploadProps,
                D3D12_HEAP_FLAG_NONE,
                &ibDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&sm.ibUpload));

            sm.ibUpload->Map(0, &range, &mapped);
            memcpy(mapped, indices.data(), ibSize);
            sm.ibUpload->Unmap(0, nullptr);

            cmdList->CopyBufferRegion(sm.ib, 0, sm.ibUpload, 0, ibSize);

            CD3DX12_RESOURCE_BARRIER ibBarrier =
                CD3DX12_RESOURCE_BARRIER::Transition(
                    sm.ib,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    D3D12_RESOURCE_STATE_INDEX_BUFFER);
            cmdList->ResourceBarrier(1, &ibBarrier);

            sm.ibView.BufferLocation = sm.ib->GetGPUVirtualAddress();
            sm.ibView.SizeInBytes = ibSize;
            sm.ibView.Format = DXGI_FORMAT_R32_UINT;
        }
    }

    std::ostringstream log;
    log << "[FBX] Mesh Loaded: " << filename << "\n"
        << "   BaseMeshIndex: " << baseMeshIndex << "\n"
        << "   SubMeshes: " << m_SubMeshes.size() << "\n"
        << "   Bones    : " << m_Bones.size() << "\n";
    OutputDebugStringA(log.str().c_str());

    mgr->Destroy();
}



void CMesh::EnableSkinning(int nBones)
{
    // [추가] 뼈가 없으면 스키닝 대상이 아님
    if (nBones <= 0)
    {
        m_bSkinnedMesh = false;
        // 기존 버퍼는 그대로 두거나, 확실히 비우고 싶다면 Release 해도 됨
        return;
    }

    m_bSkinnedMesh = true;

    //  기존 코드 계속...
    // 기존 m_pxmf4x4BoneTransforms 정리
    if (m_pxmf4x4BoneTransforms)
        delete[] m_pxmf4x4BoneTransforms;

    m_pxmf4x4BoneTransforms = new XMFLOAT4X4[nBones];

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    for (int i = 0; i < nBones; ++i)
        m_pxmf4x4BoneTransforms[i] = identity;

    UINT cbSize = (UINT)(sizeof(XMFLOAT4X4) * nBones);

    if (m_pd3dcbBoneTransforms)
    {
        m_pd3dcbBoneTransforms->Release();
        m_pd3dcbBoneTransforms = nullptr;
    }

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer((cbSize + 255) & ~255);

    HRESULT hr = m_pd3dDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_pd3dcbBoneTransforms)
    );

    if (FAILED(hr))
    {
        OutputDebugStringA("[EnableSkinning] Failed to create bone CB.\n");
        m_bSkinnedMesh = false;
        return;
    }

    void* pMapped = nullptr;
    m_pd3dcbBoneTransforms->Map(0, nullptr, &pMapped);
    memcpy(pMapped, m_pxmf4x4BoneTransforms, cbSize);
    m_pd3dcbBoneTransforms->Unmap(0, nullptr);

    std::ostringstream log;
    log << "[EnableSkinning] Bone CB created. Bones: " << nBones << "\n";
    OutputDebugStringA(log.str().c_str());
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

//==========================================================================
// Animator Helper Functions
//==========================================================================

// 본 개수 반환
int CMesh::GetBoneCount() const
{
    return static_cast<int>(m_Bones.size());
}

// 본 이름으로 인덱스 검색
int CMesh::GetBoneIndexByName(const std::string& boneName) const
{
    auto it = m_BoneNameToIndex.find(boneName);
    if (it == m_BoneNameToIndex.end()) return -1;
    return it->second;
}

// 본의 부모 인덱스 반환
int CMesh::GetBoneParentIndex(int boneIndex) const
{
    if (boneIndex < 0 || boneIndex >= static_cast<int>(m_Bones.size()))
        return -1;

    return m_Bones[boneIndex].parentIndex;
}

// 애니메이터가 없으면 생성하고 스켈레톤 전달
CAnimator* CMesh::EnsureAnimator()
{
    if (!m_pAnimator)
    {
        m_pAnimator = new CAnimator();
        m_pAnimator->SetSkeleton(m_Bones, m_BoneNameToIndex);
    }
    return m_pAnimator;
}

//---------------------------------------------------------------------------
// Bone CBV 관련
//---------------------------------------------------------------------------

// 애니메이션 결과 본 행렬을 GPU 상수버퍼에 업로드
void CMesh::UpdateBoneTransformsOnGPU(
    ID3D12GraphicsCommandList* cmdList,
    const XMFLOAT4X4* boneMatrices,
    int nBones)
{
    (void)cmdList;

    if (!m_bSkinnedMesh) return;
    if (!m_pd3dcbBoneTransforms) return;
    if (!boneMatrices) return;
    if (nBones <= 0) return;

    const int boneCount = static_cast<int>(m_Bones.size());
    if (boneCount <= 0) return;
    if (nBones > boneCount) nBones = boneCount;

    const UINT copySize = sizeof(XMFLOAT4X4) * nBones;

    // 1) Transpose 해서 월드와 같은 규약으로 맞춤
    std::vector<XMFLOAT4X4> transposed(nBones);
    for (int i = 0; i < nBones; ++i)
    {
        XMMATRIX m = XMLoadFloat4x4(&boneMatrices[i]);
        XMMATRIX t = XMMatrixTranspose(m);
        XMStoreFloat4x4(&transposed[i], t);
    }

    if (m_pxmf4x4BoneTransforms)
        memcpy(m_pxmf4x4BoneTransforms, transposed.data(), copySize);

    // 2) GPU 상수 버퍼에 업로드
    void* pMapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = m_pd3dcbBoneTransforms->Map(0, &readRange, &pMapped);
    if (FAILED(hr) || !pMapped)
    {
        OutputDebugStringA("[UpdateBoneTransformsOnGPU] Map failed.\n");
        return;
    }

    memcpy(pMapped, transposed.data(), copySize);
    m_pd3dcbBoneTransforms->Unmap(0, nullptr);
}



void CMesh::FillSkinWeights(FbxMesh* mesh, SubMesh& sm)
{
    if (!mesh) return;

    const int cpCount = mesh->GetControlPointsCount();
    if (cpCount <= 0) return;

    // 기존 내용 초기화 (혹시라도 다른 값이 들어있었다면)
    sm.boneIndices.clear();
    sm.boneWeights.clear();

    // -------------------------------------------------------------------------
    // 1) ControlPoint(정점)마다 어떤 Bone이 몇 %로 영향을 주는지 수집
    // -------------------------------------------------------------------------
    // cpInfluences[cpIndex] = { (boneIndex, weight), ... }
    std::vector<std::vector<std::pair<int, double>>> cpInfluences(cpCount);

    const int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int s = 0; s < skinCount; ++s)
    {
        FbxSkin* skin = FbxCast<FbxSkin>(mesh->GetDeformer(s, FbxDeformer::eSkin));
        if (!skin) continue;

        const int clusterCount = skin->GetClusterCount();
        for (int c = 0; c < clusterCount; ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            if (!cluster) continue;

            FbxNode* linkNode = cluster->GetLink(); // 이 클러스터가 가리키는 본 노드
            if (!linkNode) continue;

            const char* boneName = linkNode->GetName();
            auto it = m_BoneNameToIndex.find(boneName);
            if (it == m_BoneNameToIndex.end())
                continue; // 이 본은 스켈레톤(Bone 배열)에 없음

            const int boneIndex = it->second;

            const int* indices = cluster->GetControlPointIndices();
            const double* weights = cluster->GetControlPointWeights();
            const int    indexCount = cluster->GetControlPointIndicesCount();

            for (int i = 0; i < indexCount; ++i)
            {
                int cpIdx = indices[i];
                if (cpIdx < 0 || cpIdx >= cpCount) continue;

                double w = weights[i];
                if (w <= 0.0) continue;

                cpInfluences[cpIdx].emplace_back(boneIndex, w);
            }
        }
    }

    // -------------------------------------------------------------------------
    // 2) 각 ControlPoint마다 최대 4개까지 영향이 큰 본만 유지하고, 가중치 정규화
    // -------------------------------------------------------------------------
    // 2) cp별로 최대 4개 본만 유지하고, 가중치 정규화
    std::vector<XMUINT4>  cpBones(cpCount, XMUINT4(0, 0, 0, 0));
    // 스킨 정보 없는 정점은 weight 전부 0 > VS에서 fallback 경로 사용
    std::vector<XMFLOAT4> cpWeights(cpCount, XMFLOAT4(0, 0, 0, 0));

    for (int cp = 0; cp < cpCount; ++cp)
    {
        auto& infl = cpInfluences[cp];
        if (infl.empty())
        {
            // 스킨 인플루언스 없음 > boneIndices=0, weights=0,0,0,0
            // VS 스키닝 셰이더에서 weight 합=0이면 skinnedPos==0이 되어
            // fallback(원래 posL / normal) 경로로 처리되게 한다.
            cpBones[cp] = XMUINT4(0, 0, 0, 0);
            cpWeights[cp] = XMFLOAT4(0, 0, 0, 0);
            continue;
        }

        // weight 내림차순 정렬
        std::sort(infl.begin(), infl.end(),
            [](const std::pair<int, double>& a, const std::pair<int, double>& b)
            {
                return a.second > b.second;
            });

        int useCount = (infl.size() < 4) ? (int)infl.size() : 4;

        double sum = 0.0;
        for (int i = 0; i < useCount; ++i)
            sum += infl[i].second;

        if (sum <= 0.0)
            continue;

        XMUINT4 bi(0, 0, 0, 0);
        XMFLOAT4 bw(0, 0, 0, 0);

        for (int i = 0; i < useCount; ++i)
        {
            const int   b = infl[i].first;
            const float w = (float)(infl[i].second / sum); // 정규화

            switch (i)
            {
            case 0:
                bi.x = b; bw.x = w; break;
            case 1:
                bi.y = b; bw.y = w; break;
            case 2:
                bi.z = b; bw.z = w; break;
            case 3:
                bi.w = b; bw.w = w; break;
            }
        }

        // 혹시 합이 1이 안 될 수도 있으니, 마지막에 한번 보정(선택사항)
        float totalW = bw.x + bw.y + bw.z + bw.w;
        if (totalW > 0.0f && fabsf(totalW - 1.0f) > 1e-3f)
        {
            bw.x /= totalW;
            bw.y /= totalW;
            bw.z /= totalW;
            bw.w /= totalW;
        }

        cpBones[cp] = bi;
        cpWeights[cp] = bw;
    }

    // -------------------------------------------------------------------------
    // 3) polygon 순회를 다시 하면서, SubMesh 정점 순서에 맞춰 boneIndices / boneWeights push
    //    (positions / normals / uvs를 채울 때와 동일한 순서로 순회해야 한다)
    // -------------------------------------------------------------------------
    const int polyCount = mesh->GetPolygonCount();
    if (polyCount <= 0) return;

    // xform / flip은 geometry 만들 때와 같은 기준으로 다시 계산
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

    sm.boneIndices.reserve(sm.positions.size());
    sm.boneWeights.reserve(sm.positions.size());

    for (int p = 0; p < polyCount; ++p)
    {
        int order[3] = { 0, 1, 2 };
        if (flip) std::swap(order[1], order[2]);

        for (int i = 0; i < 3; ++i)
        {
            int v = order[i];
            int cpIdx = mesh->GetPolygonVertex(p, v);
            if (cpIdx < 0 || cpIdx >= cpCount)
            {
                // 잘못된 인덱스면 안전하게 기본값 사용
                sm.boneIndices.push_back(XMUINT4(0, 0, 0, 0));
                sm.boneWeights.push_back(XMFLOAT4(1, 0, 0, 0));
                continue;
            }

            sm.boneIndices.push_back(cpBones[cpIdx]);
            sm.boneWeights.push_back(cpWeights[cpIdx]);
        }
    }

    // 여기까지 오면 sm.positions.size() == sm.boneIndices.size() == sm.boneWeights.size() 가 되는 것이 정상
}

//----------------------------------------------------------------------------
// 애니메이션 FBX → AnimationClip 로드
//----------------------------------------------------------------------------
bool CMesh::LoadAnimationFromFBX(
    const char* filename,
    const std::string& clipName,
    AnimationClip& outClip,
    float timeScale)
{
    if (!filename) return false;
    if (m_Bones.empty())
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromFBX] Skeleton is empty.\n");
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // 1) FBX Manager / Scene
    // ---------------------------------------------------------------------------------------
    FbxManager* mgr = FbxManager::Create();
    FbxIOSettings* ios = FbxIOSettings::Create(mgr, IOSROOT);
    mgr->SetIOSettings(ios);

    FbxImporter* imp = FbxImporter::Create(mgr, "");
    if (!imp->Initialize(filename, -1, mgr->GetIOSettings()))
    {
        OutputDebugStringA("[LoadAnimation] Importer Initialize failed.\n");
        imp->Destroy();
        mgr->Destroy();
        return false;
    }

    FbxScene* scene = FbxScene::Create(mgr, "AnimScene");
    imp->Import(scene);
    imp->Destroy();

    // 동일한 좌표계 적용 (Mesh 로드와 동일)
    FbxAxisSystem::DirectX.ConvertScene(scene);
    FbxSystemUnit::m.ConvertScene(scene);

    // ---------------------------------------------------------------------------------------
    // 2) Animation Stack / Layer
    // ---------------------------------------------------------------------------------------
    FbxAnimStack* stack = scene->GetCurrentAnimationStack();
    if (!stack && scene->GetSrcObjectCount<FbxAnimStack>() > 0)
        stack = scene->GetSrcObject<FbxAnimStack>(0);

    if (!stack)
    {
        OutputDebugStringA("[LoadAnimation] No AnimStack.\n");
        scene->Destroy();
        mgr->Destroy();
        return false;
    }

    FbxTimeSpan timeSpan = stack->GetLocalTimeSpan();
    FbxAnimLayer* layer = stack->GetMember<FbxAnimLayer>(0);

    if (!layer)
    {
        OutputDebugStringA("[LoadAnimation] No AnimLayer.\n");
        scene->Destroy();
        mgr->Destroy();
        return false;
    }

    // ---------------------------------------------------------------------------------------
    // 3) Clip metadata
    // ---------------------------------------------------------------------------------------
    outClip.name = clipName.empty() ? std::string(stack->GetName()) : clipName;

    double startSec = timeSpan.GetStart().GetSecondDouble();
    double endSec = timeSpan.GetStop().GetSecondDouble();
    outClip.duration = (float)((endSec - startSec) * timeScale);

    outClip.boneTracks.clear();
    outClip.boneNameToTrack.clear();
    outClip.boneTracks.resize(m_Bones.size());

    // 트랙 기본 세팅
    for (size_t i = 0; i < m_Bones.size(); ++i)
    {
        BoneKeyframes& track = outClip.boneTracks[i];
        track.boneIndex = (int)i;
        track.boneName = m_Bones[i].name;
        outClip.boneNameToTrack[track.boneName] = (int)i;
    }

    // 스케일 제거용 헬퍼
    auto RemoveScale = [](XMMATRIX m)
        {
            XMVECTOR s, r, t;
            XMMatrixDecompose(&s, &r, &t, m);
            // 스케일을 1,1,1로 만들고 회전+이동만 남긴다.
            return XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(t);
        };

    // ============================================================
    // 4) 애니메이션 rest pose(local) 추출 + deltaLocal 계산
    // ============================================================
    {
        FbxTime restTime = timeSpan.GetStart();   // 클립 시작 프레임

        for (size_t i = 0; i < m_Bones.size(); ++i)
        {
            FbxNode* boneNode = scene->FindNodeByName(m_Bones[i].name.c_str());
            if (!boneNode)
            {
                XMStoreFloat4x4(&m_Bones[i].animRestLocal, XMMatrixIdentity());
                XMStoreFloat4x4(&m_Bones[i].deltaLocal, XMMatrixIdentity());
                continue;
            }

            // 애니 rest pose (local)
            FbxAMatrix restLocalM = boneNode->EvaluateLocalTransform(restTime);
            FbxVector4 RT = restLocalM.GetT();
            FbxQuaternion RR = restLocalM.GetQ();
            FbxVector4 RS = restLocalM.GetS();

            XMMATRIX animRest =
                XMMatrixScaling((float)RS[0], (float)RS[1], (float)RS[2]) *
                XMMatrixRotationQuaternion(XMVectorSet((float)RR[0], (float)RR[1], (float)RR[2], (float)RR[3])) *
                XMMatrixTranslation((float)RT[0], (float)RT[1], (float)RT[2]);

            // ▼ 스케일 제거 버전으로 animRestLocal 저장
            XMMATRIX animRestNoScale = RemoveScale(animRest);
            XMStoreFloat4x4(&m_Bones[i].animRestLocal, animRestNoScale);

            // *** 핵심: deltaLocal = (bindLocal_noScale) * inverse(animRest_noScale) ***
            XMMATRIX bindLocal = XMLoadFloat4x4(&m_Bones[i].bindLocal);
            XMMATRIX bindNoScale = RemoveScale(bindLocal);

            XMMATRIX delta =
                bindNoScale * XMMatrixInverse(nullptr, animRestNoScale);

            XMStoreFloat4x4(&m_Bones[i].deltaLocal, delta);
        }
    }

    // ============================================================
// 5) 키프레임 추출 (deltaLocal * rawLocal 보정 적용)
//      + 0프레임 제거(첫 유효 프레임을 0초로 재정렬)
// ============================================================
    std::function<void(FbxNode*)> traverse = [&](FbxNode* node)
        {
            if (!node) return;

            auto it = m_BoneNameToIndex.find(node->GetName());
            if (it != m_BoneNameToIndex.end())
            {
                int boneIndex = it->second;
                XMMATRIX delta = XMLoadFloat4x4(&m_Bones[boneIndex].deltaLocal);

                std::set<FbxTime> keyTimes;
                CollectKeyTimes(node, layer, keyTimes);

                BoneKeyframes& track = outClip.boneTracks[boneIndex];

                for (FbxTime t : keyTimes)
                {
                    if (t < timeSpan.GetStart() || t > timeSpan.GetStop())
                        continue;

                    // raw local (애니가 가진 원래 로컬 변환)
                    FbxAMatrix rawM = node->EvaluateLocalTransform(t);
                    FbxVector4 T = rawM.GetT();
                    FbxQuaternion R = rawM.GetQ();
                    FbxVector4 S = rawM.GetS();

                    XMMATRIX rawLocal =
                        XMMatrixScaling((float)S[0], (float)S[1], (float)S[2]) *
                        XMMatrixRotationQuaternion(XMVectorSet((float)R[0], (float)R[1], (float)R[2], (float)R[3])) *
                        XMMatrixTranslation((float)T[0], (float)T[1], (float)T[2]);

                    // ===== rest pose → 모델 bind pose 정렬 =====
                    XMMATRIX corrected = delta * rawLocal;

                    // corrected → TRS 분해
                    XMVECTOR outS, outR, outT;
                    XMMatrixDecompose(&outS, &outR, &outT, corrected);

                    Keyframe k;
                    k.timeSec = (float)((t.GetSecondDouble() - startSec) * timeScale);

                    XMStoreFloat3(&k.translation, outT);
                    XMStoreFloat4(&k.rotationQuat, outR);
                    XMStoreFloat3(&k.scale, outS);

                    track.keyframes.push_back(k);
                }

                // 시간순 정렬
                std::sort(track.keyframes.begin(), track.keyframes.end(),
                    [](const Keyframe& a, const Keyframe& b)
                    {
                        return a.timeSec < b.timeSec;
                    });
            }

            for (int c = 0; c < node->GetChildCount(); ++c)
                traverse(node->GetChild(c));
        };

    traverse(scene->GetRootNode());

    // ============================================================
    // ★ (추가) 0프레임(T포즈) 제거: 전체 트랙에서 가장 작은 timeSec > 0 찾기
    // ============================================================
    float minTime = FLT_MAX;

    for (auto& track : outClip.boneTracks)
    {
        for (auto& k : track.keyframes)
        {
            if (k.timeSec > 0.0f && k.timeSec < minTime)
            {
                minTime = k.timeSec;
            }
        }
    }


    // minTime이 유효할 경우 → 모든 key.timeSec -= minTime
    if (minTime != FLT_MAX)
    {
        for (auto& track : outClip.boneTracks)
        {
            for (auto& k : track.keyframes)
                k.timeSec -= minTime;
        }

        // duration도 갱신
        outClip.duration -= minTime;
    }


    // ---------------------------------------------------------------------------------------
    // 6) cleanup
    // ---------------------------------------------------------------------------------------
    scene->Destroy();
    mgr->Destroy();

    return true;
}
