//-----------------------------------------------------------------------------
// File: Shader.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Shader.h"

CShader::CShader()
{
}

CShader::~CShader()
{
	if (m_pd3dPipelineState) m_pd3dPipelineState->Release();
	m_pd3dPipelineState = NULL;

	ReleaseShaderVariables();
}

D3D12_SHADER_BYTECODE CShader::CreateVertexShader(ID3DBlob **ppd3dShaderBlob)
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = NULL;

	return(d3dShaderByteCode);
}

D3D12_SHADER_BYTECODE CShader::CreatePixelShader(ID3DBlob **ppd3dShaderBlob)
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = NULL;

	return(d3dShaderByteCode);
}

D3D12_SHADER_BYTECODE CShader::CompileShaderFromFile(WCHAR *pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob **ppd3dShaderBlob)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	::D3DCompileFromFile(pszFileName, NULL, NULL, pszShaderName, pszShaderProfile, nCompileFlags, 0, ppd3dShaderBlob, NULL);

	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	return(d3dShaderByteCode);
}

D3D12_INPUT_LAYOUT_DESC CShader::CreateInputLayout()
{
	static D3D12_INPUT_ELEMENT_DESC d3dInputElementDescs[] =
	{
		// 위치
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			// 법선
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

				// 텍스처 좌표
				{ "TEXTURECOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
					24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc = {};
	d3dInputLayoutDesc.pInputElementDescs = d3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = _countof(d3dInputElementDescs);
	return d3dInputLayoutDesc;
}

D3D12_RASTERIZER_DESC CShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC d3dRasterizerDesc;
	::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	d3dRasterizerDesc.FrontCounterClockwise = FALSE;
	d3dRasterizerDesc.DepthBias = 0;
	d3dRasterizerDesc.DepthBiasClamp = 0.0f;
	d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
	d3dRasterizerDesc.DepthClipEnable = TRUE;
	d3dRasterizerDesc.MultisampleEnable = FALSE;
	d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
	d3dRasterizerDesc.ForcedSampleCount = 0;
	d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return(d3dRasterizerDesc);
}

D3D12_DEPTH_STENCIL_DESC CShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	::ZeroMemory(&d3dDepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
	d3dDepthStencilDesc.DepthEnable = TRUE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0x00;
	d3dDepthStencilDesc.StencilWriteMask = 0x00;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	return(d3dDepthStencilDesc);
}

D3D12_BLEND_DESC CShader::CreateBlendState()
{
	D3D12_BLEND_DESC d3dBlendDesc = {};
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;

	auto& rt = d3dBlendDesc.RenderTarget[0];
	rt.BlendEnable = TRUE;                          //  블렌딩 활성화
	rt.LogicOpEnable = FALSE;

	rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;            //  source = texColor.a
	rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;       //  dest = 1 - texColor.a
	rt.BlendOp = D3D12_BLEND_OP_ADD;

	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return d3dBlendDesc;
}


void CShader::CreateShader(ID3D12Device *pd3dDevice, ID3D12RootSignature *pd3dGraphicsRootSignature)
{
	ID3DBlob *pd3dVertexShaderBlob = NULL, *pd3dPixelShaderBlob = NULL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dPipelineStateDesc;
	::ZeroMemory(&d3dPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	d3dPipelineStateDesc.pRootSignature = pd3dGraphicsRootSignature;
	d3dPipelineStateDesc.VS = CreateVertexShader(&pd3dVertexShaderBlob);
	d3dPipelineStateDesc.PS = CreatePixelShader(&pd3dPixelShaderBlob);
	d3dPipelineStateDesc.RasterizerState = CreateRasterizerState();
	d3dPipelineStateDesc.BlendState = CreateBlendState();
	d3dPipelineStateDesc.DepthStencilState = CreateDepthStencilState();
	d3dPipelineStateDesc.InputLayout = CreateInputLayout();
	d3dPipelineStateDesc.SampleMask = UINT_MAX;
	d3dPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d3dPipelineStateDesc.NumRenderTargets = 1;
	d3dPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dPipelineStateDesc.SampleDesc.Count = 1;
	d3dPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	pd3dDevice->CreateGraphicsPipelineState(&d3dPipelineStateDesc, __uuidof(ID3D12PipelineState), (void **)&m_pd3dPipelineState);

	if (pd3dVertexShaderBlob) pd3dVertexShaderBlob->Release();
	if (pd3dPixelShaderBlob) pd3dPixelShaderBlob->Release();

	if (d3dPipelineStateDesc.InputLayout.pInputElementDescs) delete[] d3dPipelineStateDesc.InputLayout.pInputElementDescs;
}

void CShader::CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CShader::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
}

void CShader::ReleaseShaderVariables()
{
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList)
{
	if (m_pd3dPipelineState) pd3dCommandList->SetPipelineState(m_pd3dPipelineState);
}

void CShader::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	OnPrepareRender(pd3dCommandList);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CLightingShader::CLightingShader()
{
}

CLightingShader::~CLightingShader()
{
}

D3D12_INPUT_LAYOUT_DESC CLightingShader::CreateInputLayout()
{
	static D3D12_INPUT_ELEMENT_DESC d3dInputElementDescs[] =
	{
		// position (float3)
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			// normal (float3)
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

				// uv(float2)
				{ "TEXTURECOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
					24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

					// bone indices (uint4)
					{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0,
						32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

						// bone weights (float4)
						{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
							48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc = {};
	d3dInputLayoutDesc.pInputElementDescs = d3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = _countof(d3dInputElementDescs);
	return d3dInputLayoutDesc;
}

D3D12_SHADER_BYTECODE CLightingShader::CreateVertexShader(ID3DBlob **ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSLighting", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CLightingShader::CreatePixelShader(ID3DBlob **ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSLighting", "ps_5_1", ppd3dShaderBlob));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
D3D12_INPUT_LAYOUT_DESC CSkinnedLightingShader::CreateInputLayout()
{
	// POSITION(12) + NORMAL(12) + UV(8) + BI(16) + BW(16) = 64 bytes
	D3D12_INPUT_ELEMENT_DESC* d = new D3D12_INPUT_ELEMENT_DESC[5];

	d[0] = { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,      0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d[1] = { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,      0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d[2] = { "TEXTURECOORD", 0, DXGI_FORMAT_R32G32_FLOAT,         0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d[3] = { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,    0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d[4] = { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,   0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC desc{};
	desc.pInputElementDescs = d;
	desc.NumElements = 5;
	return desc;
}

D3D12_SHADER_BYTECODE CSkinnedLightingShader::CreateVertexShader(ID3DBlob** pp)
{
	// VSLightingSkinned 사용
	return CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSLightingSkinned", "vs_5_1", pp);
}

D3D12_SHADER_BYTECODE CSkinnedLightingShader::CreatePixelShader(ID3DBlob** pp)
{
	return CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSLighting", "ps_5_1", pp);
}
