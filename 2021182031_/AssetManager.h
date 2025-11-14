#pragma once

enum class AssetType
{
	UnityChan,
	BoxMan,
	Robot,
	Orc,
	Unknown
};
inline std::wstring ToWstring(const std::string& s)
{
	return std::wstring(s.begin(), s.end());
}

std::string GetTextureFileNameForSubMesh_UnityChan(const SubMesh& sm);
std::string GetTextureFileNameForSubMesh_BoxMan(const SubMesh& sm);

std::string GetTextureFileNameForSubMesh(const SubMesh& sm, AssetType type);