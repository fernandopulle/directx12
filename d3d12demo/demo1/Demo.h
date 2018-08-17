#pragma once

#include <DirectXMath.h>
#include "DGraphics.h"


class Demo : public DGraphics {
public:
	Demo(UINT bufferCount, std::string name, LONG width, LONG height);
	void render();

private:
	void createConstantBuffer();
	void createRootSignature();
	void createPipelineStateWireframe();
	void createPipelineStateSolid();
	Microsoft::WRL::ComPtr<ID3D12PipelineState> createPipelineState(D3D12_FILL_MODE fillMode, D3D12_CULL_MODE cullMode);

private:
	const int numParts{ 28 };

	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateWireframe;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateSolid;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> currPipelineState;


	int tessFactor{ 8 };
};