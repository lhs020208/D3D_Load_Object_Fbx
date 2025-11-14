#include "stdafx.h"
#include "Mesh.h"
#include "AssetManager.h"

/*
1) SubMesh.materialName

FBX 머티리얼 이름을 그대로 사용

Unity FBX에서 흔히 Body, HairMat, FaceMat, Costume 등으로 존재

2) SubMesh.meshName

Blender/Max에서 오브젝트 이름

BoxMan: "Head", "EyeWhite", "Body", "LeftArm" …

3) 텍스처 파일명 규칙

각 에셋의 텍스처 PNG 이름을 직접 보고 규칙을 만든다
(ex: hair.png, body.png, body_diffuse.png …)
*/

std::string GetTextureFileNameForSubMesh_UnityChan(const SubMesh& sm)
{
    std::string mat = sm.materialName;
    std::string mesh = sm.meshName;

    // 소문자로 변환
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
        };

    mat = lower(mat);
    mesh = lower(mesh);

    // -----------------------------
    // Hair
    // -----------------------------
    if (mat.find("hair") != std::string::npos ||
        mesh.find("hair") != std::string::npos)
        return "hair_01.png";

    // -----------------------------
    // Body / Skin
    // -----------------------------
    if (mat.find("body") != std::string::npos ||
        mesh.find("body") != std::string::npos)
        return "body_01.png";

    // -----------------------------
    // Face / Eye
    // -----------------------------
    if (mat.find("face") != std::string::npos ||
        mesh.find("face") != std::string::npos)
        return "face_00.png";


    // -----------------------------
    // eye
    // -----------------------------
    if (mat.find("eyeline") != std::string::npos ||
        mesh.find("eyeline") != std::string::npos)
        return "eyeline_00.png";

    if (mat.find("eye_R") != std::string::npos ||
        mesh.find("eye_R") != std::string::npos)
        return "eye_iris_R_00.png";

    if (mat.find("eye") != std::string::npos  ||
        mesh.find("eye") != std::string::npos)
        return "eye_iris_L_00.png";

    // -----------------------------
    // mat
    // -----------------------------
    if (mat.find("mat_cheek") != std::string::npos ||
        mesh.find("mat_cheek") != std::string::npos)
        return "cheek_00.png";

    // Fallback
    return "skin_01.png";
}
std::string GetTextureFileNameForSubMesh_BoxMan(const SubMesh& sm)
{
    std::string mat = sm.materialName;
    std::string mesh = sm.meshName;

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
        };

    mat = lower(mat);
    mesh = lower(mesh);

    // ========= 1) Skin / Body =========
    if (mat.find("body") != std::string::npos ||
        mesh.find("body") != std::string::npos)
        return "body.png";

    // ========= 2) Face / Eye =========
    if (mat.find("eye") != std::string::npos ||
        mesh.find("head") != std::string::npos)
        return "face.png";

    // ========= 3) Clothes =========
    if (mat.find("cloth") != std::string::npos ||
        mesh.find("pants") != std::string::npos)
        return "cloth.png";

    // 기본값
    return "default.png";
}

std::string GetTextureFileNameForSubMesh(const SubMesh& sm, AssetType type)
{
    switch (type)
    {
    case AssetType::UnityChan:
        return GetTextureFileNameForSubMesh_UnityChan(sm);

    case AssetType::BoxMan:
        return GetTextureFileNameForSubMesh_BoxMan(sm);

    //case AssetType::Robot:
    //    return GetTextureFileNameForSubMesh_Robot(sm);

    //case AssetType::Orc:
    //    return GetTextureFileNameForSubMesh_Orc(sm);

    default:
        return "default.png";
    }
}
