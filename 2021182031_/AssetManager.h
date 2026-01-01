#pragma once

enum class AssetType
{
	UnityChan,
	House,
	Robot,
	Orc,
	Unknown
};
inline std::wstring ToWstring(const std::string& s)
{
	return std::wstring(s.begin(), s.end());
}

std::string GetTextureFileNameForSubMesh_UnityChan(const SubMesh& sm);
std::string GetTextureFileNameForSubMesh_House(const SubMesh& sm);
std::string GetTextureFileNameForSubMesh_Orc(const SubMesh& sm);

std::string GetTextureFileNameForSubMesh(const SubMesh& sm, AssetType type);