//-----------------------------------------------------------------------------
// File: CGameObject.cpp
//-----------------------------------------------------------------------------

#include <fstream>
#include <cstdint>
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

CMesh::CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName, int FileType)
{
    m_pd3dDevice = pd3dDevice;
	if (pstrFileName) {
		if (FileType == 1) LoadMeshFromBIN(pd3dDevice, pd3dCommandList, pstrFileName);
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

void CMesh::LoadMeshFromBIN(ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    const char* filename)
{
    // ----------------------------------------------------
    // 0) 파일 열기
    // ----------------------------------------------------
    std::ifstream fin(filename, std::ios::binary);
    if (!fin.is_open()) return;

    auto ReadRaw = [&](void* dst, size_t sz) -> bool
        {
            fin.read(reinterpret_cast<char*>(dst), sz);
            return fin.good();
        };

    auto ReadUInt32 = [&](uint32_t& v) -> bool { return ReadRaw(&v, sizeof(v)); };
    auto ReadInt32 = [&](int32_t& v)  -> bool { return ReadRaw(&v, sizeof(v)); };
    auto ReadUInt16 = [&](uint16_t& v) -> bool { return ReadRaw(&v, sizeof(v)); };

    auto ReadString = [&](std::string& s) -> bool
        {
            uint16_t len = 0;
            if (!ReadUInt16(len)) return false;
            if (len == 0) { s.clear(); return true; }

            s.resize(len);
            if (!ReadRaw(s.data(), len)) return false;
            return true;
        };

    // ----------------------------------------------------
    // 1) 헤더 읽기
    // ----------------------------------------------------
    char magic[4] = {};
    if (!ReadRaw(magic, 4)) return;
    if (!(magic[0] == 'M' && magic[1] == 'B' && magic[2] == 'I' && magic[3] == 'N'))
        return;

    uint32_t version = 0;
    uint32_t flags = 0;
    uint32_t boneCount = 0;
    uint32_t subMeshCount = 0;

    if (!ReadUInt32(version)) return;
    if (!ReadUInt32(flags))   return;
    if (!ReadUInt32(boneCount)) return;
    if (!ReadUInt32(subMeshCount)) return;

    if (version != 1) return; // 버전 체크

    // 기존 데이터 정리
    m_Bones.clear();
    m_BoneNameToIndex.clear();
    m_SubMeshes.clear();

    // ----------------------------------------------------
    // 2) Skeleton 섹션 → m_Bones 채우기
    // ----------------------------------------------------
    m_Bones.reserve(boneCount);

    for (uint32_t i = 0; i < boneCount; ++i)
    {
        std::string name;
        if (!ReadString(name)) return;

        int32_t parentIndex = -1;
        if (!ReadInt32(parentIndex)) return;

        float bindLocalArr[16];
        float offsetArr[16];
        if (!ReadRaw(bindLocalArr, sizeof(bindLocalArr)))  return;
        if (!ReadRaw(offsetArr, sizeof(offsetArr)))     return;

        Bone b{};
        b.name = name;
        b.parentIndex = parentIndex;

        // float[16] → XMFLOAT4X4
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                b.bindLocal.m[r][c] = bindLocalArr[r * 4 + c];
                b.offsetMatrix.m[r][c] = offsetArr[r * 4 + c];
            }
        }

        XMStoreFloat4x4(&b.animRestLocal, XMMatrixIdentity());
        XMStoreFloat4x4(&b.deltaLocal, XMMatrixIdentity());

        m_BoneNameToIndex[b.name] = static_cast<int>(m_Bones.size());
        m_Bones.push_back(b);
    }

    // ----------------------------------------------------
    // 3) SubMesh 섹션 → m_SubMeshes 채우기
    // ----------------------------------------------------
    m_SubMeshes.reserve(subMeshCount);

    for (uint32_t si = 0; si < subMeshCount; ++si)
    {
        SubMesh sm{};

        // 3-1) meshName / materialName
        if (!ReadString(sm.meshName))     return;
        if (!ReadString(sm.materialName)) return;

        // 3-2) vertexCount / indexCount
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        if (!ReadUInt32(vertexCount)) return;
        if (!ReadUInt32(indexCount))  return;

        sm.positions.reserve(vertexCount);
        sm.normals.reserve(vertexCount);
        sm.uvs.reserve(vertexCount);
        sm.boneIndices.reserve(vertexCount);
        sm.boneWeights.reserve(vertexCount);
        sm.indices.reserve(indexCount);

        // 3-3) 정점 데이터
        for (uint32_t v = 0; v < vertexCount; ++v)
        {
            float pos[3];
            float nml[3];
            float uv[2];
            uint32_t bi[4];
            float bw[4];

            if (!ReadRaw(pos, sizeof(pos)))       return;
            if (!ReadRaw(nml, sizeof(nml)))       return;
            if (!ReadRaw(uv, sizeof(uv)))        return;
            if (!ReadRaw(bi, sizeof(bi)))        return;
            if (!ReadRaw(bw, sizeof(bw)))        return;

            XMFLOAT3 p(pos[0], pos[1], pos[2]);
            XMFLOAT3 n(nml[0], nml[1], nml[2]);
            XMFLOAT2 t(uv[0], uv[1]);

            XMUINT4  boneIdx(bi[0], bi[1], bi[2], bi[3]);
            XMFLOAT4 boneW(bw[0], bw[1], bw[2], bw[3]);

            sm.positions.push_back(p);
            sm.normals.push_back(n);
            sm.uvs.push_back(t);
            sm.boneIndices.push_back(boneIdx);
            sm.boneWeights.push_back(boneW);
        }

        // 3-4) 인덱스 데이터
        for (uint32_t ii = 0; ii < indexCount; ++ii)
        {
            uint32_t idx = 0;
            if (!ReadUInt32(idx)) return;
            sm.indices.push_back(idx);
        }

        m_SubMeshes.push_back(std::move(sm));
    }

    fin.close();

    // ----------------------------------------------------
    // 4) 스키닝 여부 판단
    // ----------------------------------------------------
    if (!m_Bones.empty())
    {
        // FBX 로더에서 하던 것처럼 본 개수만 넘겨서 CBV 생성
        EnableSkinning(static_cast<int>(m_Bones.size()));
    }

    // ----------------------------------------------------
    // 5) GPU Vertex/Index Buffer 생성
    //     → LoadMeshFromFBX()의 10) 부분 그대로 재사용
    // ----------------------------------------------------
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
        CD3DX12_RESOURCE_DESC   vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

        // Vertex Buffer (Default)
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

        // Index Buffer
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

bool CMesh::LoadAnimationFromBIN(
    const char* filename,
    const std::string& clipName,
    AnimationClip& outClip,
    float timeScale)
{
    if (!filename) return false;
    if (m_Bones.empty())
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromBIN] Skeleton is empty.\n");
        return false;
    }

    std::ifstream fin(filename, std::ios::binary);
    if (!fin.is_open())
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromBIN] Cannot open file.\n");
        return false;
    }

    auto ReadRaw = [&](void* dst, size_t sz) -> bool
        {
            fin.read(reinterpret_cast<char*>(dst), sz);
            return fin.good();
        };

    auto ReadUInt32 = [&](uint32_t& v) -> bool
        {
            return ReadRaw(&v, sizeof(v));
        };

    auto ReadInt32 = [&](int32_t& v) -> bool
        {
            return ReadRaw(&v, sizeof(v));
        };

    auto ReadFloat = [&](float& f) -> bool
        {
            return ReadRaw(&f, sizeof(f));
        };

    auto ReadString = [&](std::string& s) -> bool
        {
            uint16_t len = 0;
            if (!ReadRaw(&len, sizeof(len))) return false;
            if (len == 0)
            {
                s.clear();
                return true;
            }
            s.resize(len);
            if (!ReadRaw(&s[0], len)) return false;
            return true;
        };

    // --------------------------------------------------------
    // 1) Header
    // --------------------------------------------------------
    char magic[4] = {};
    if (!ReadRaw(magic, 4))
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromBIN] Failed to read magic.\n");
        return false;
    }
    if (memcmp(magic, "ABIN", 4) != 0)
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromBIN] Invalid magic.\n");
        return false;
    }

    uint32_t version = 0;
    if (!ReadUInt32(version)) return false;
    if (version != 1)
    {
        OutputDebugStringA("[CMesh::LoadAnimationFromBIN] Unsupported version.\n");
        return false;
    }

    // --------------------------------------------------------
    // 2) Clip metadata
    // --------------------------------------------------------
    std::string fileClipName;
    if (!ReadString(fileClipName)) return false;

    float duration = 0.0f;
    if (!ReadFloat(duration)) return false;

    uint32_t trackCount = 0;
    if (!ReadUInt32(trackCount)) return false;

    // AnimationClip 기본 세팅
    outClip.name = !clipName.empty() ? clipName : fileClipName;
    outClip.duration = duration * timeScale;

    outClip.boneTracks.clear();
    outClip.boneNameToTrack.clear();
    outClip.boneTracks.resize(m_Bones.size());

    for (size_t i = 0; i < m_Bones.size(); ++i)
    {
        BoneKeyframes& track = outClip.boneTracks[i];
        track.boneIndex = static_cast<int>(i);
        track.boneName = m_Bones[i].name;
        outClip.boneNameToTrack[track.boneName] = static_cast<int>(i);
    }

    // --------------------------------------------------------
    // 3) Tracks 로드
    //    BIN에는 "키가 있는 노드"만 저장되어 있으므로
    //    boneName으로 CMesh의 m_BoneNameToIndex 에 매핑한다.
    // --------------------------------------------------------
    for (uint32_t t = 0; t < trackCount; ++t)
    {
        std::string binBoneName;
        if (!ReadString(binBoneName)) return false;

        int32_t binBoneIndex = -1;
        if (!ReadInt32(binBoneIndex)) return false; // 현재는 사용하지 않음

        uint32_t keyCount = 0;
        if (!ReadUInt32(keyCount)) return false;

        // skeleton 과 매칭
        auto itBone = m_BoneNameToIndex.find(binBoneName);
        bool mapped = (itBone != m_BoneNameToIndex.end());
        BoneKeyframes* pTrack = nullptr;
        if (mapped)
            pTrack = &outClip.boneTracks[itBone->second];

        for (uint32_t k = 0; k < keyCount; ++k)
        {
            float timeSec, tx, ty, tz, rx, ry, rz, rw, sx, sy, sz;

            if (!ReadFloat(timeSec)) return false;
            if (!ReadFloat(tx)) return false;
            if (!ReadFloat(ty)) return false;
            if (!ReadFloat(tz)) return false;

            if (!ReadFloat(rx)) return false;
            if (!ReadFloat(ry)) return false;
            if (!ReadFloat(rz)) return false;
            if (!ReadFloat(rw)) return false;

            if (!ReadFloat(sx)) return false;
            if (!ReadFloat(sy)) return false;
            if (!ReadFloat(sz)) return false;

            if (!mapped)
                continue; // 데이터는 읽었지만, 스켈레톤에 없는 노드면 버림

            Keyframe kf;
            kf.timeSec = timeSec * timeScale;
            kf.translation = XMFLOAT3(tx, ty, tz);
            kf.rotationQuat = XMFLOAT4(rx, ry, rz, rw);
            kf.scale = XMFLOAT3(sx, sy, sz);

            pTrack->keyframes.push_back(kf);
        }
    }

    fin.close();

    // --------------------------------------------------------
    // 4) 각 본별 키프레임 정렬 (안전용)
    // --------------------------------------------------------
    for (auto& track : outClip.boneTracks)
    {
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const Keyframe& a, const Keyframe& b)
            {
                return a.timeSec < b.timeSec;
            });
    }

    return true;
}
