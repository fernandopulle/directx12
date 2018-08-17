#pragma once
#include "d3dx12.h"
#include <DirectXMath.h>
#include "DGraphics.h"

class Demo : public DGraphics {
public:
	Demo(UINT bufferCount, std::string name, LONG width, LONG height);
	void render();

private:
	void createShaders();
	void createRootSignature();
	void createPipelineStateWireframe();
	void createPipelineStateSolid();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> createPipelineState(D3D12_FILL_MODE fillMode, D3D12_CULL_MODE cullMode);
	void createViewport();
	void createScissorRect();
private:
	const int numParts{ 28 };

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW  indexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> hullShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> domainShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateWireframe;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateSolid;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> currPipelineState;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	int tessFactor{ 8 };
};