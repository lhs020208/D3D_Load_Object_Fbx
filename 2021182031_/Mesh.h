//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------
#pragma once
#include "stdafx.h"
#include "AnimatorData.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//class FbxMesh;
class CVertex
{
public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
    CVertex(float x, float y, float z) { m_xmf3Position = XMFLOAT3(x, y, z); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() { }

	XMFLOAT3						m_xmf3Position;
};

class CPolygon
{
public:
    CPolygon() {}
    CPolygon(int nVertices);
    ~CPolygon();

    int							m_nVertices = 0;
    CVertex* m_pVertices = NULL;

    void SetVertex(int nIndex, CVertex& vertex);
};

class CDiffusedVertex : public CVertex {
public:
	XMFLOAT4 m_xmf4Diffuse;
	XMFLOAT3 m_xmf3Normal;
	CDiffusedVertex() : m_xmf4Diffuse(0, 0, 0, 0), m_xmf3Normal(0, 1, 0) {}
	CDiffusedVertex(XMFLOAT3 pos, XMFLOAT4 dif, XMFLOAT3 normal) : CVertex(pos), m_xmf4Diffuse(dif), m_xmf3Normal(normal) {}
};

struct SubMesh
{
	vector<XMFLOAT3> positions;
	vector<XMFLOAT3> normals;
	vector<XMFLOAT2> uvs;
	vector<XMUINT4>  boneIndices;
	vector<XMFLOAT4> boneWeights;
	vector<UINT>     indices;

	UINT textureIndex = UINT_MAX;       // 이 SubMesh가 사용할 텍스처의 SRV 인덱스

	std::string meshName;
	std::string materialName;

	// GPU 리소스
	ID3D12Resource* vb = nullptr;
	ID3D12Resource* vbUpload = nullptr;
	ID3D12Resource* ib = nullptr;
	ID3D12Resource* ibUpload = nullptr;

	ID3D12Resource* texture = nullptr;
	ID3D12Resource* textureUpload = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	D3D12_INDEX_BUFFER_VIEW  ibView{};
};



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CAnimator;

class CMesh
{
public:
	CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName = NULL, int FileType = 1);
	virtual ~CMesh();

private:
	int								m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }
	void SetSrvDescriptorInfo(ID3D12DescriptorHeap* heap, UINT inc);

	void ReleaseUploadBuffers();
    void SetPolygon(int nIndex, CPolygon* pPolygon);
	int CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance = nullptr);
	BOOL RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance);


	BoundingBox						m_xmBoundingBox;
	BoundingOrientedBox			    m_xmOOBB = BoundingOrientedBox();



protected:
	UINT							m_nVertices = 0;
	
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;

    int							    m_nPolygons = 0;
    CPolygon                        **m_ppPolygons = NULL;

	vector<Bone>					m_Bones;            // 본 정보 배열
	unordered_map<string, int>		m_BoneNameToIndex;  // 본 이름→인덱스 매핑

	XMUINT4*						m_pxu4BoneIndices = NULL;   // 정점별 본 인덱스 (최대 4)
	
	XMFLOAT4*						m_pxmf4BoneWeights = NULL;  // 정점별 본 가중치 (최대 4)
	XMFLOAT4X4*						m_pxmf4x4BoneTransforms = NULL;  // 최종 본 행렬
	ID3D12Resource*					m_pd3dcbBoneTransforms = NULL; // GPU용 상수 버퍼

	ID3D12Resource*					m_pd3dBoneIndexBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneIndexUploadBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightUploadBuffer = NULL;

	CAnimator*						m_pAnimator = nullptr;   // 애니메이션 관리자
	bool							m_bSkinnedMesh = false;        // 스키닝 메시 여부

	ID3D12DescriptorHeap			*m_pd3dSrvDescriptorHeap = nullptr;
	UINT							m_nSrvDescriptorIncrementSize = 0;
	UINT							m_nTextureRootParameterIndex = 5;  // t0이 RootParam5라고 가정
	ID3D12Device*					m_pd3dDevice = nullptr;

public:
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList);
	
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const {
		return m_pd3dcbBoneTransforms
			? m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
			: 0;
	}

	void LoadMeshFromBIN(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename);
	void EnableSkinning(int nBones);

	void LoadTextureFromFile(ID3D12Device* device,ID3D12GraphicsCommandList* cmdList,
		ID3D12DescriptorHeap* srvHeap,UINT descriptorIndex,const wchar_t* fileName,int subMeshIndex);
	
	vector<SubMesh> m_SubMeshes;

	// CAnimator 연동을 위한 헬퍼 함수들
	// 본 개수 반환
	int GetBoneCount() const;

	// 본 이름으로 인덱스를 찾는 함수
	// 존재하지 않으면 -1 반환
	int GetBoneIndexByName(const std::string& boneName) const;

	// 본의 부모 인덱스 반환
	int GetBoneParentIndex(int boneIndex) const;

	// 전체 본 배열 반환 (읽기 전용)
	const std::vector<Bone>& GetBones() const { return m_Bones; }

	// 애니메이터 포인터 반환
	CAnimator* GetAnimator() const { return m_pAnimator; }

	// 애니메이터가 없으면 자동 생성하여 반환
	CAnimator* EnsureAnimator();

	// --- Bone CBV 관련 헬퍼 ---

	void UpdateBoneTransformsOnGPU(ID3D12GraphicsCommandList* cmdList,
		const XMFLOAT4X4* boneMatrices,
		int nBones);

	bool IsSkinnedMesh() const { return m_bSkinnedMesh; }
	bool HasBoneCB() const { return (m_pd3dcbBoneTransforms != nullptr); }

	bool LoadAnimationFromBIN(const char* filename,
		const std::string& clipName,
		AnimationClip& outClip,
		float timeScale = 1.0f);


	CAnimator* GetAnimator() { return m_pAnimator; }
	bool HasAnimator() const { return m_pAnimator != nullptr; }

};