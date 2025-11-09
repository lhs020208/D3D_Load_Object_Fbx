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

CMesh::CMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, char *pstrFileName)
{
	if (pstrFileName) LoadMeshFromFile(pd3dDevice, pd3dCommandList, pstrFileName);
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
}

void CMesh::ReleaseUploadBuffers() 
{
	if (m_pd3dPositionUploadBuffer) m_pd3dPositionUploadBuffer->Release();
	if (m_pd3dNormalUploadBuffer) m_pd3dNormalUploadBuffer->Release();
	if (m_pd3dTextureCoordUploadBuffer) m_pd3dTextureCoordUploadBuffer->Release();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer->Release();

	m_pd3dPositionUploadBuffer = NULL;
	m_pd3dNormalUploadBuffer = NULL;
	m_pd3dTextureCoordUploadBuffer = NULL;
	m_pd3dIndexUploadBuffer = NULL;
};

void CMesh::Render(ID3D12GraphicsCommandList *pd3dCommandList)
{
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);
	pd3dCommandList->IASetVertexBuffers(m_nSlot, m_nVertexBufferViews, m_pd3dVertexBufferViews);
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

void CMesh::LoadMeshFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, char* filename)
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

/////////////////////////////////////////////////////////////////////////////////////////////////////

CCubeMesh::CCubeMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
	float fWidth, float fHeight, float fDepth) : CMesh(6)
{
	// 1. 정점 설정
	float w = fWidth * 0.5f, h = fHeight * 0.5f, d = fDepth * 0.5f;

	XMFLOAT3 vertices[] = {
		{-w, +h, -d}, {+w, +h, -d}, {+w, -h, -d}, {-w, -h, -d}, // Front
		{-w, +h, +d}, {+w, +h, +d}, {+w, -h, +d}, {-w, -h, +d}  // Back
	};
	m_nVertices = _countof(vertices);
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];
	memcpy(m_pxmf3Positions, vertices, sizeof(vertices));

	// 2. 인덱스 설정 (삼각형 12개 → 36개 인덱스)
	UINT indices[] = {
		0,1,2, 0,2,3,  // Front
		4,6,5, 4,7,6,  // Back
		4,5,1, 4,1,0,  // Top
		3,2,6, 3,6,7,  // Bottom
		4,0,3, 4,3,7,  // Left
		1,5,6, 1,6,2   // Right
	};
	m_nIndices = _countof(indices);
	m_pnIndices = new UINT[m_nIndices];
	memcpy(m_pnIndices, indices, sizeof(indices));

	// 3. 정점 버퍼 생성
	UINT vbSize = sizeof(XMFLOAT3) * m_nVertices;
	m_pd3dPositionBuffer = CreateBufferResource(device, cmdList, m_pxmf3Positions, vbSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pd3dPositionUploadBuffer);

	m_nVertexBufferViews = 1;
	m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
	m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_pd3dVertexBufferViews[0].StrideInBytes = sizeof(XMFLOAT3);
	m_pd3dVertexBufferViews[0].SizeInBytes = vbSize;

	// 4. 인덱스 버퍼 생성
	UINT ibSize = sizeof(UINT) * m_nIndices;
	m_pd3dIndexBuffer = CreateBufferResource(device, cmdList, m_pnIndices, ibSize,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = ibSize;

	m_xmOOBB.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmOOBB.Extents = XMFLOAT3(fWidth * 0.5f, fHeight * 0.5f, fDepth * 0.5f);
}


CCubeMesh::~CCubeMesh()
{
}
////////////////////////////////////////////////////////////////////////////////////////////////
CHeightMapImage::CHeightMapImage(LPCTSTR pFileName, int nWidth, int nLength, XMFLOAT3
	xmf3Scale)
{
	m_nWidth = nWidth;
	m_nLength = nLength;
	m_xmf3Scale = xmf3Scale;
	BYTE* pHeightMapPixels = new BYTE[m_nWidth * m_nLength];
	HANDLE hFile = ::CreateFile(pFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
	FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY, NULL);
	DWORD dwBytesRead;
	::ReadFile(hFile, pHeightMapPixels, (m_nWidth * m_nLength), &dwBytesRead, NULL);
	::CloseHandle(hFile);
	m_pHeightMapPixels = new BYTE[m_nWidth * m_nLength];
	for (int y = 0; y < m_nLength; y++)
	{
		for (int x = 0; x < m_nWidth; x++)
		{
			m_pHeightMapPixels[x + ((m_nLength - 1 - y) * m_nWidth)] = pHeightMapPixels[x +
				(y * m_nWidth)];
		}
	}
	if (pHeightMapPixels) delete[] pHeightMapPixels;
}
CHeightMapImage::~CHeightMapImage()
{
	if (m_pHeightMapPixels) delete[] m_pHeightMapPixels;
	m_pHeightMapPixels = NULL;
}

XMFLOAT3 CHeightMapImage::GetHeightMapNormal(int x, int z)
{
	if ((x < 0.0f) || (z < 0.0f) || (x >= m_nWidth) || (z >= m_nLength))
	return(XMFLOAT3(0.0f, 1.0f, 0.0f));
	int nHeightMapIndex = x + (z * m_nWidth);
	int xHeightMapAdd = (x < (m_nWidth - 1)) ? 1 : -1;
	int zHeightMapAdd = (z < (m_nLength - 1)) ? m_nWidth : -m_nWidth;
	float y1 = (float)m_pHeightMapPixels[nHeightMapIndex] * m_xmf3Scale.y;
	float y2 = (float)m_pHeightMapPixels[nHeightMapIndex + xHeightMapAdd] * m_xmf3Scale.y;
	float y3 = (float)m_pHeightMapPixels[nHeightMapIndex + zHeightMapAdd] * m_xmf3Scale.y;
	XMFLOAT3 xmf3Edge1 = XMFLOAT3(0.0f, y3 - y1, m_xmf3Scale.z);
	XMFLOAT3 xmf3Edge2 = XMFLOAT3(m_xmf3Scale.x, y2 - y1, 0.0f);
	XMFLOAT3 xmf3Normal = Vector3::CrossProduct(xmf3Edge1, xmf3Edge2, true);
	return(xmf3Normal);
}

#define _WITH_APPROXIMATE_OPPOSITE_CORNER
float CHeightMapImage::GetHeight(float fx, float fz)
{
	if ((fx < 0.0f) || (fz < 0.0f) || (fx >= m_nWidth) || (fz >= m_nLength)) return(0.0f);
	int x = (int)fx;
	int z = (int)fz;
	float fxPercent = fx - x;
	float fzPercent = fz - z;
	float fBottomLeft = (float)m_pHeightMapPixels[x + (z * m_nWidth)];
	float fBottomRight = (float)m_pHeightMapPixels[(x + 1) + (z * m_nWidth)];
	float fTopLeft = (float)m_pHeightMapPixels[x + ((z + 1) * m_nWidth)];
	float fTopRight = (float)m_pHeightMapPixels[(x + 1) + ((z + 1) * m_nWidth)];
#ifdef _WITH_APPROXIMATE_OPPOSITE_CORNER
	bool bRightToLeft = ((z % 2) != 0);
	if (bRightToLeft)
	{
		if (fzPercent >= fxPercent)
			fBottomRight = fBottomLeft + (fTopRight - fTopLeft);
		else
			fTopLeft = fTopRight + (fBottomLeft - fBottomRight);
	}
	else
	{
		if (fzPercent < (1.0f - fxPercent))
			fTopRight = fTopLeft + (fBottomRight - fBottomLeft);
		else
			fBottomLeft = fTopLeft + (fBottomRight - fTopRight);
	}
#endif
	float fTopHeight = fTopLeft * (1 - fxPercent) + fTopRight * fxPercent;
	float fBottomHeight = fBottomLeft * (1 - fxPercent) + fBottomRight * fxPercent;
	float fHeight = fBottomHeight * (1 - fzPercent) + fTopHeight * fzPercent;
	return(fHeight);
}

CHeightMapGridMesh::CHeightMapGridMesh(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList, int xStart, int zStart, int nWidth, int
	nLength, XMFLOAT3 xmf3Scale, XMFLOAT4 xmf4Color, void* pContext) : CMesh(pd3dDevice,
		pd3dCommandList)
{
	m_nVertices = nWidth * nLength;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	m_nWidth = nWidth;
	m_nLength = nLength;
	m_xmf3Scale = xmf3Scale;
	CDiffusedVertex* pVertices = new CDiffusedVertex[m_nVertices];

	float fHeight = 0.0f, fMinHeight = +FLT_MAX, fMaxHeight = -FLT_MAX;
	for (int i = 0, z = zStart; z < (zStart + nLength); z++)
	{
		for (int x = xStart; x < (xStart + nWidth); x++, i++)
		{
			XMFLOAT3 xmf3Position = XMFLOAT3((x * m_xmf3Scale.x), OnGetHeight(x, z, pContext),
				(z * m_xmf3Scale.z));
			XMFLOAT4 xmf3Color = Vector4::Add(OnGetColor(x, z, pContext), xmf4Color);
			XMFLOAT3 normal = ((CHeightMapImage*)pContext)->GetHeightMapNormal(x, z);
			pVertices[i] = CDiffusedVertex(xmf3Position, xmf3Color, normal);
			if (fHeight < fMinHeight) fMinHeight = fHeight;
			if (fHeight > fMaxHeight) fMaxHeight = fHeight;
		}
	}
	m_pd3dPositionBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pVertices, sizeof(CDiffusedVertex) * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dPositionUploadBuffer
	);

	m_nVertexBufferViews = 1;
	if (m_pd3dVertexBufferViews) delete[] m_pd3dVertexBufferViews;
	m_pd3dVertexBufferViews = new D3D12_VERTEX_BUFFER_VIEW[1];
	m_pd3dVertexBufferViews[0].BufferLocation = m_pd3dPositionBuffer->GetGPUVirtualAddress();
	m_pd3dVertexBufferViews[0].StrideInBytes = m_nStride;
	m_pd3dVertexBufferViews[0].SizeInBytes = m_nStride * m_nVertices;

	if (m_pxmf3Positions) delete[] m_pxmf3Positions;
	m_pxmf3Positions = new XMFLOAT3[m_nVertices];
	for (int i = 0; i < m_nVertices; i++)
		m_pxmf3Positions[i] = pVertices[i].m_xmf3Position;

	delete[] pVertices;
	m_nIndices = ((nWidth * 2) * (nLength - 1)) + ((nLength - 1) - 1);
	UINT* pnIndices = new UINT[m_nIndices];
	for (int j = 0, z = 0; z < nLength - 1; z++)
	{
		if ((z % 2) == 0)
		{
			for (int x = 0; x < nWidth; x++)
			{
				if ((x == 0) && (z > 0)) pnIndices[j++] = (UINT)(x + (z * nWidth));
				pnIndices[j++] = (UINT)(x + (z * nWidth));
				pnIndices[j++] = (UINT)((x + (z * nWidth)) + nWidth);
			}
		}
		else
		{
			for (int x = nWidth - 1; x >= 0; x--)
			{
				if (x == (nWidth - 1)) pnIndices[j++] = (UINT)(x + (z * nWidth));
				pnIndices[j++] = (UINT)(x + (z * nWidth));
				pnIndices[j++] = (UINT)((x + (z * nWidth)) + nWidth);
			}
		}
	}
	m_pd3dIndexBuffer = ::CreateBufferResource(pd3dDevice, pd3dCommandList, pnIndices,
		sizeof(UINT) * m_nIndices, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
	delete[] pnIndices;
}
CHeightMapGridMesh::~CHeightMapGridMesh()
{
}

float CHeightMapGridMesh::OnGetHeight(int x, int z, void* pContext)
{
	CHeightMapImage* pHeightMapImage = (CHeightMapImage*)pContext;
	BYTE* pHeightMapPixels = pHeightMapImage->GetHeightMapPixels();
	XMFLOAT3 xmf3Scale = pHeightMapImage->GetScale();
	int nWidth = pHeightMapImage->GetHeightMapWidth();
	float fHeight = pHeightMapPixels[x + (z * nWidth)] * xmf3Scale.y;
	return(fHeight);
}

XMFLOAT4 CHeightMapGridMesh::OnGetColor(int x, int z, void* pContext)
{
	XMFLOAT3 xmf3LightDirection = XMFLOAT3(-1.0f, 1.0f, 1.0f);
	xmf3LightDirection = Vector3::Normalize(xmf3LightDirection);
	CHeightMapImage* pHeightMapImage = (CHeightMapImage*)pContext;
	XMFLOAT3 xmf3Scale = pHeightMapImage->GetScale();
	XMFLOAT4 xmf4IncidentLightColor(0.9f, 0.8f, 0.4f, 1.0f);
	float fScale = Vector3::DotProduct(pHeightMapImage->GetHeightMapNormal(x, z),
		xmf3LightDirection);
	fScale += Vector3::DotProduct(pHeightMapImage->GetHeightMapNormal(x + 1, z),
		xmf3LightDirection);
	fScale += Vector3::DotProduct(pHeightMapImage->GetHeightMapNormal(x + 1, z + 1),
		xmf3LightDirection);
	fScale += Vector3::DotProduct(pHeightMapImage->GetHeightMapNormal(x, z + 1),
		xmf3LightDirection);
	fScale = (fScale / 4.0f) + 0.05f;
	if (fScale > 1.0f) fScale = 1.0f;
	if (fScale < 0.25f) fScale = 0.25f;
	XMFLOAT4 xmf4Color = Vector4::Multiply(fScale, xmf4IncidentLightColor);
	return(xmf4Color);
}
