//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"
#include "Player.h"
#include "AssetManager.h"

class CScene
{
public:
	CScene() {}
    CScene(CPlayer* pPlayer);
    ~CScene();

	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	virtual void BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, 
		ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap, UINT m_nSrvDescriptorIncrementSize);
	virtual void ReleaseObjects();

	ID3D12RootSignature *CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature *GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature); }

	bool ProcessInput();
	virtual void Animate(float fTimeElapsed);
	void PrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera=NULL, ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap = NULL);

	virtual void ReleaseUploadBuffers();
	void SetPlayer(CPlayer* pPlayer) { m_pPlayer = pPlayer; }
	void BuildGraphicsRootSignature(ID3D12Device* pd3dDevice);
	void CreateLightConstantBuffer(ID3D12Device* device);

	ID3D12Resource* m_pDefaultBoneCB = nullptr; // b4용 디폴트 CBV
	ID3D12Resource* m_pLightCB = nullptr;

	D3D12_GPU_VIRTUAL_ADDRESS GetDefaultBoneCBAddress() const {
		return m_pDefaultBoneCB ? m_pDefaultBoneCB->GetGPUVirtualAddress() : 0;
	}
protected:
	CPlayer* m_pPlayer = NULL;
protected:
	ID3D12RootSignature			*m_pd3dGraphicsRootSignature = NULL;

};


class CTankScene : public CScene {
public:
	CTankScene() {}
	CTankScene(CPlayer* pPlayer);
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, 
		ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap, UINT m_nSrvDescriptorIncrementSize) override;
	virtual void ReleaseObjects() override;
	virtual void ReleaseUploadBuffers();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, ID3D12DescriptorHeap* m_pd3dSrvDescriptorHeap) override;
	virtual void Animate(float fElapsedTime) override;

	virtual void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;
	CGameObject* PickObjectPointedByCursor(int xClient, int yClient, CCamera* pCamera);
	virtual void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;

private:
	XMFLOAT3 m_xmf3LightDirection = XMFLOAT3(0.0f, -1.0f, 0.0f);
	XMFLOAT3 m_xmf3LightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
};