//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------

#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
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

struct Bone
{
	std::string name;            // 본 이름
	int parentIndex;             // 부모 본 인덱스
	XMFLOAT4X4 offsetMatrix;     // Inverse Bind Pose (모델 공간 → 본 공간)
};

struct SkinnedVertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	UINT boneIndices[4];     // 어떤 본들이 영향을 주는가
	float boneWeights[4];    // 각 본의 영향 비율
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

	void ReleaseUploadBuffers();
    void SetPolygon(int nIndex, CPolygon* pPolygon);
	int CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance = nullptr);
	BOOL RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance);


	BoundingBox						m_xmBoundingBox;
	BoundingOrientedBox			    m_xmOOBB = BoundingOrientedBox();
protected:
	UINT							m_nVertices = 0;
	XMFLOAT3						*m_pxmf3Positions = NULL;
	ID3D12Resource					*m_pd3dPositionBuffer = NULL;
	ID3D12Resource					*m_pd3dPositionUploadBuffer = NULL;

	XMFLOAT3						*m_pxmf3Normals = NULL;
	ID3D12Resource					*m_pd3dNormalBuffer = NULL;
	ID3D12Resource					*m_pd3dNormalUploadBuffer = NULL;

	XMFLOAT2						*m_pxmf2TextureCoords = NULL;
	ID3D12Resource					*m_pd3dTextureCoordBuffer = NULL;
	ID3D12Resource					*m_pd3dTextureCoordUploadBuffer = NULL;

	UINT							m_nIndices = 0;
	UINT							*m_pnIndices = NULL;
	ID3D12Resource					*m_pd3dIndexBuffer = NULL;
	ID3D12Resource					*m_pd3dIndexUploadBuffer = NULL;

	UINT							m_nVertexBufferViews = 0;
	D3D12_VERTEX_BUFFER_VIEW		*m_pd3dVertexBufferViews = NULL;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dVertexBufferView;

	D3D12_INDEX_BUFFER_VIEW			m_d3dIndexBufferView;

	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;
	UINT							m_nStride = 0;
	UINT							m_nOffset = 0;

	UINT							m_nStartIndex = 0;
	int								m_nBaseVertex = 0;

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

	// ----------------------
	// Texture (SRV) 관련 멤버
	// ----------------------
	ID3D12Resource* m_pd3dTexture = nullptr;
	ID3D12Resource* m_pd3dTextureUploadBuffer = nullptr;
	// 이 Mesh의 텍스처가 DescriptorHeap(SRV Heap)에서 점유하는 슬롯 번호
	UINT                m_nTextureDescriptorIndex = UINT_MAX;

public:
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList);

	void LoadMeshFromOBJ(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName);
	void LoadMeshFromFBX(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename);
	void EnableSkinning(int nBones);
};