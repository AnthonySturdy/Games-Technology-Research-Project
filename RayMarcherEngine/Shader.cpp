#include "pch.h"
#include "Shader.h"

Shader::Shader(ID3D11Device* device, const WCHAR* vertexShaderPathWithoutExt, const WCHAR* pixelShaderPathWithoutExt, D3D11_INPUT_ELEMENT_DESC* vertexLayout, UINT numElements)
{
	// Compile and create Vertex shader
	ID3DBlob* vsBlob = nullptr;
	std::wstring vertShaderHlsl(vertexShaderPathWithoutExt);
	vertShaderHlsl += L".hlsl";
	if (FAILED(CompileShaderFromFile(vertShaderHlsl.c_str(), "VS", "vs_5_0", &vsBlob)))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled. Ensure this executable is relative to the specified file path.", L"Error", MB_OK);
		return;
	}

	if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_VertexShader)))
	{
		vsBlob->Release();
		return;
	}

	// Compile and create Pixel shader
	ID3DBlob* psBlob = nullptr;
	std::wstring pixelShaderHlsl(pixelShaderPathWithoutExt);
	pixelShaderHlsl += L".hlsl";
	if (FAILED(CompileShaderFromFile(pixelShaderHlsl.c_str(), "PS", "ps_5_0", &psBlob)))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled. Ensure this executable is relative to the specified file path.", L"Error", MB_OK);
		return;
	}

	if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_PixelShader)))
	{
		vsBlob->Release();
		psBlob->Release();
		return;
	}

	// Create the input layout
	if (FAILED(device->CreateInputLayout(vertexLayout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_VertexLayout)))
	{
		vsBlob->Release();
		psBlob->Release();
		return;
	}

	// Release blobs
	vsBlob->Release();
	psBlob->Release();
}

HRESULT Shader::CompileShaderFromFile(const WCHAR* fileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** blobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Improves shader debugging experience but still allows for optimisation.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging.
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(fileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel,
	                        dwShaderFlags, 0, blobOut, &pErrorBlob);

	if (FAILED(hr))
	{
		// Output if D3DCompileFromFile has error
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}

		return hr;
	}

	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}
