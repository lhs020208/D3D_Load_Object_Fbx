#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cassert>

#include <fbxsdk.h>
using namespace std;

// ==========================================================
// 전역 저장 데이터 (FBX 파싱 후 여기에 채움)
// ==========================================================

struct Bone
{
    std::string name;
    int32_t parentIndex = -1;
    float bindLocal[16];
    float offsetMatrix[16];
};

struct Vertex
{
    float position[3];
    float normal[3];
    float uv[2];
    uint32_t boneIndices[4];
    float boneWeights[4];
};

struct SubMesh
{
    std::string meshName;
    std::string materialName;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

vector<Bone> g_Bones;
vector<SubMesh> g_SubMeshes;

// ==========================================================
// 파일 출력 스트림 (전역)
// ==========================================================

static std::ofstream g_out;

// ==========================================================
// Raw write helpers
// ==========================================================

void WriteRaw(const void* data, size_t size)
{
    g_out.write(reinterpret_cast<const char*>(data), size);
}

void WriteUInt16(uint16_t v) { WriteRaw(&v, sizeof(v)); }
void WriteUInt32(uint32_t v) { WriteRaw(&v, sizeof(v)); }
void WriteInt32(int32_t v) { WriteRaw(&v, sizeof(v)); }

void WriteFloatArray(const float* f, size_t count)
{
    WriteRaw(f, sizeof(float) * count);
}

void WriteStringUtf8(const std::string& s)
{
    uint16_t len = static_cast<uint16_t>(s.size());
    WriteUInt16(len);
    if (len > 0)
        WriteRaw(s.data(), len);
}

// ==========================================================
// 1) 파일 헤더 저장
// ==========================================================

void WriteModelHeader()
{
    char magic[4] = { 'M', 'B', 'I', 'N' };
    WriteRaw(magic, 4);

    uint32_t version = 1;
    uint32_t flags = 0;
    uint32_t boneCount = (uint32_t)g_Bones.size();
    uint32_t subCount = (uint32_t)g_SubMeshes.size();

    WriteUInt32(version);
    WriteUInt32(flags);
    WriteUInt32(boneCount);
    WriteUInt32(subCount);
}

// ==========================================================
// 2) Skeleton 섹션 저장
// ==========================================================

void WriteSkeletonSection()
{
    for (auto& b : g_Bones)
    {
        WriteStringUtf8(b.name);
        WriteInt32(b.parentIndex);
        WriteFloatArray(b.bindLocal, 16);
        WriteFloatArray(b.offsetMatrix, 16);
    }
}

// ==========================================================
// 3) SubMesh 섹션 저장
// ==========================================================

void WriteSubMeshSection()
{
    for (auto& sm : g_SubMeshes)
    {
        // 이름들
        WriteStringUtf8(sm.meshName);
        WriteStringUtf8(sm.materialName);

        // 개수
        uint32_t vtxCount = (uint32_t)sm.vertices.size();
        uint32_t idxCount = (uint32_t)sm.indices.size();
        WriteUInt32(vtxCount);
        WriteUInt32(idxCount);

        // 정점
        for (const Vertex& v : sm.vertices)
        {
            WriteFloatArray(v.position, 3);
            WriteFloatArray(v.normal, 3);
            WriteFloatArray(v.uv, 2);
            WriteRaw(v.boneIndices, sizeof(uint32_t) * 4);
            WriteFloatArray(v.boneWeights, 4);
        }

        // 인덱스
        if (idxCount > 0)
            WriteRaw(sm.indices.data(), sizeof(uint32_t) * idxCount);
    }
}

// ==========================================================
// BIN 파일 저장 함수
// ==========================================================

bool SaveModelBin(const std::string& filename)
{
    g_out.open(filename, ios::binary);
    if (!g_out.is_open()) return false;

    WriteModelHeader();
    WriteSkeletonSection();
    WriteSubMeshSection();

    g_out.close();
    return true;
}

// ==========================================================
// ★★★★★ FBX 파싱 함수 (추후 단계에서 구현) ★★★★★
// 지금은 틀만 만들고 내용은 비워둔다.
// ==========================================================

void ExtractFromFBX(FbxScene* scene)
{
    // ==================================================
    // TODO: 이 함수가 LoadMeshFromFBX()가 하던 일을 한다.
    //
    // 1) Skeleton 추출 → g_Bones 채우기
    // 2) Mesh/ SubMesh 추출 → g_SubMeshes 채우기
    // 3) Skin Weights 채우기
    //
    // 지금은 저장기 구조만 만들고,
    // 실제 FBX 데이터 파싱은 이후 단계에서 순차적으로 만든다.
    // ==================================================
}

// ==========================================================
// main
// ==========================================================

int main()
{
    cout << "FBX 파일 이름을 입력하십시오.(ex: model): ";
    string s;
    cin >> s;

    string fbxFileName = s + ".fbx";
    string binFileName = s + ".bin";

    // FBX SDK 초기화
    FbxManager* manager = FbxManager::Create();
    if (!manager)
    {
        cout << "FBX Manager 생성 실패.\n";
        return -1;
    }

    FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(ios);

    FbxImporter* importer = FbxImporter::Create(manager, "");
    bool ok = importer->Initialize(fbxFileName.c_str(), -1, manager->GetIOSettings());

    if (!ok)
    {
        cout << "FBX 파일을 열 수 없습니다: " << fbxFileName << "\n";
        importer->Destroy();
        manager->Destroy();
        return -1;
    }

    cout << "FBX 파일 열기 성공: " << fbxFileName << "\n";

    FbxScene* scene = FbxScene::Create(manager, "scene");
    importer->Import(scene);
    importer->Destroy();

    // ================================
    // FBX → RAM 구조체 추출 (TODO)
    // ================================
    ExtractFromFBX(scene);

    // ================================
    // BIN 저장
    // ================================
    if (SaveModelBin(binFileName))
        cout << "BIN 파일 생성 완료: " << binFileName << "\n";
    else
        cout << "BIN 파일 생성 실패\n";

    manager->Destroy();
    return 0;
}
