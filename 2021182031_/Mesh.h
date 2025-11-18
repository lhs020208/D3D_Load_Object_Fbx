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

	// FBX Skin 정보를 SubMesh의 boneIndices / boneWeights에 채우는 헬퍼
	void FillSkinWeights(FbxMesh* mesh, SubMesh& sm);

public:
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList);
	
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const {
		return m_pd3dcbBoneTransforms
			? m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
			: 0;
	}

	void LoadMeshFromOBJ(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName);
	void LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename);
	void EnableSkinning(int nBones);

	//void LoadTextureFromFile(ID3D12Device* device,ID3D12GraphicsCommandList* cmdList,
	//	ID3D12DescriptorHeap* srvHeap,UINT descriptorIndex,const wchar_t* fileName);
	// 구버전. 서브메시 아닌 경우에만 사용 가능
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

	// ------------------------------------------------------------
	//  애니메이션 FBX 파일 하나에서 AnimationClip 생성
	//  - filename : 애니메이션 FBX 경로 (예: "Models/unitychan_JUMP00.fbx")
	//  - clipName : AnimationClip.name 에 들어갈 이름 (예: "Jump")
	//               빈 문자열이면 FBX AnimStack 이름을 사용하도록 구현 예정
	//  - outClip  : FBX에서 추출한 키프레임/트랙/길이 정보를 채워서 반환
	//  - timeScale: FBX 시간을 초 단위로 바꾼 뒤 추가로 곱해줄 배율(기본 1.0f)
	//  반환값     : 로딩 성공 시 true, 실패 시 false
	// ------------------------------------------------------------
	bool LoadAnimationFromFBX(const char* filename,
		const std::string& clipName,
		AnimationClip& outClip,
		float timeScale = 1.0f);
};