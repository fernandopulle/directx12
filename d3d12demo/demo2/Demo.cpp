#include <stdexcept>
#include <d3dcompiler.h>
#include "Demo.h"
#include "Window.h"
#include "Utils.h"

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;


Demo::Demo(UINT bufferCount, string name, LONG width, LONG height) :DGraphics{ bufferCount, name, width, height } {

	//points 
	std::vector<DirectX::XMFLOAT3> points{ { 0.0f, 0.5f, 0.5f } ,{ 0.5f, -0.5f, 0.5f },{ -0.5f, -0.5f, 0.5f } };	
	

	using PointType = decltype(points)::value_type;

	vertexBuffer = DemoUtil::createVertexBuffer(device.Get(), points, L"vertices of triangle");
	
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = static_cast<UINT>(sizeof(PointType));
	vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferView.StrideInBytes * points.size());

	createShaders();  // create shaders
	createRootSignature();  // create root signatures

	// we create two pipeline states 

	createPipelineStateWireframe();  // wireframe
	createPipelineStateSolid();      // solid

	createViewport();
	createScissorRect();

	auto lambda = [this](WPARAM wParam)
	{
		switch (wParam)
		{
		case 49:
			currPipelineState = pipelineStateWireframe;
			break;
		case 50:
			currPipelineState = pipelineStateSolid;
			break;
		}
	};
	shared_ptr<function<void(WPARAM)>> onKeyPress = make_shared<function<void(WPARAM)>>(lambda);
	window->addKeyPressCallback(onKeyPress);
}




void Demo::render() {

	/* get the current index of the back buffer. */
	UINT frameIndex{ swapChain->GetCurrentBackBufferIndex() };

	ComPtr<ID3D12CommandAllocator> commandAllocator{ commandAllocators[frameIndex] };

	/* Reset the command alocator. This was used by GPU previously. */
	if (FAILED(commandAllocator->Reset()))
	{
		throw(runtime_error{ "Error resetting command allocator." });
	}

	/* reset the command list. by resetting the command list we are putting it into
	   a recording state so we can start recording commands into the command allocator.
	   the command allocator that we reference here may have multiple command lists
	   associated with it, but only one can be recording at any time. Make sure
	   that any other command lists associated to this command allocator are in
	   the closed state (not recording).
	   Here you will pass an initial pipeline state object as the second parameter,
	   but in this tutorial we are only clearing the rtv, and do not actually need
	   anything but an initial default pipeline, which is what we get by setting
	   the second parameter to NULL
	*/
	if (FAILED(commandList->Reset(commandAllocator.Get(), nullptr)))
	{
		throw(runtime_error{ "Error resetting command list." });
	}

	commandList->SetPipelineState(currPipelineState.Get()); // set pipeline state
	commandList->SetGraphicsRootSignature(rootSignature.Get()); // set root signature
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);


	ID3D12Resource* currBuffer{ swapChainBuffers[frameIndex].Get() };
	/*
	here we start recording commands into the commandList (which all the commands will
	be stored in the commandAllocator)
	transition the "frameIndex" render target from the present state to the render target state
	so the command list draws to it starting from here
	*/

	D3D12_RESOURCE_BARRIER barrierDesc;
	ZeroMemory(&barrierDesc, sizeof(barrierDesc));
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = currBuffer;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

	commandList->ResourceBarrier(1, &barrierDesc);

	/*  here we again get the handle to our current render target view 
	    so we can set it as the render target in the output merger stage of the pipeline
	*/
	static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv(descHeapRtv->GetCPUDescriptorHandleForHeapStart());
	descHandleRtv.ptr += frameIndex * descriptorSize;

	// set the render target for the output merger stage (the output of the pipeline)
	commandList->OMSetRenderTargets(1, &descHandleRtv, FALSE, nullptr);

	// Clear the render target by using the ClearRenderTargetView command
	static float clearColor[]{ 0.1f, 0.1f, 0.1f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
	
	// set the primitive topology
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// set the vertex buffer (using the vertex buffer view)
	vector<D3D12_VERTEX_BUFFER_VIEW> myArray{ vertexBufferView };
	commandList->IASetVertexBuffers(0, static_cast<UINT>(myArray.size()), myArray.data()); 

	commandList->DrawInstanced(3, 1, 0, 0);  // draw 3 vertices (draw the triangle)


	/*  transition the "frameIndex" render target from the render target state to the present state.
		If the debug layer is enabled, you will receive a warning if present is called on the render target when it's not in the present state
	*/
	ZeroMemory(&barrierDesc, sizeof(barrierDesc));
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = currBuffer;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrierDesc);

	// close command list
	if (FAILED(commandList->Close()))
	{
		throw(runtime_error{ "Failed closing command list." });
	}


	ID3D12CommandList* cmdList{ commandList.Get() };
	commandQueue->ExecuteCommandLists(1, &cmdList);

	if (FAILED(swapChain->Present(1, 0)))
	{
		throw(runtime_error{ "Failed present." });
	}

	UINT64& fenceValue{ fenceValues[frameIndex] };
	++fenceValue;
	ComPtr<ID3D12Fence> fence{ fences[frameIndex] };
	if (FAILED(commandQueue->Signal(fence.Get(), fenceValue)))
	{
		throw(runtime_error{ "Failed signal." });
	}

	/* Wait for frame to complete. */
	waitFrameComplete(swapChain->GetCurrentBackBufferIndex());
}


void Demo::createShaders() {

	ComPtr<ID3DBlob> error;

	if (FAILED(D3DCompileFromFile(L"VertexShader.hlsl",
		nullptr,
		nullptr,
		"main",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		vertexShaderBlob.ReleaseAndGetAddressOf(),
		error.ReleaseAndGetAddressOf()
	))) {
		throw(runtime_error{ "Error reading vertex shader." });
	}



	if (FAILED(D3DCompileFromFile(L"PixelShader.hlsl",
		nullptr,
		nullptr,
		"main",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		pixelShaderBlob.ReleaseAndGetAddressOf(),
		error.ReleaseAndGetAddressOf()))) {
		throw(runtime_error{ "Error reading pixel shader." });
	}

}


void Demo::createRootSignature() {

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.ReleaseAndGetAddressOf(), error.ReleaseAndGetAddressOf())))
	{
		throw(runtime_error{ "Error serializing root signature" });
	}

	if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating root signature" });
	}
}


void Demo::createPipelineStateWireframe()
{
	pipelineStateWireframe = createPipelineState(D3D12_FILL_MODE_WIREFRAME, D3D12_CULL_MODE_NONE);
	currPipelineState = pipelineStateWireframe;
}

void Demo::createPipelineStateSolid()
{
	pipelineStateSolid = createPipelineState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE);
}


ComPtr<ID3D12PipelineState> Demo::createPipelineState(D3D12_FILL_MODE fillMode, D3D12_CULL_MODE cullMode)
{
	vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));

	rasterizerDesc.FillMode = fillMode;
	rasterizerDesc.CullMode = cullMode;
	rasterizerDesc.FrontCounterClockwise = FALSE;
	rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizerDesc.DepthClipEnable = TRUE;
	rasterizerDesc.MultisampleEnable = FALSE;
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.ForcedSampleCount = 0;
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_BLEND_DESC blendDesc;
	ZeroMemory(&blendDesc, sizeof(blendDesc));
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0] = {
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL
	};

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depthStencilDesc.StencilEnable = FALSE;
	depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	depthStencilDesc.FrontFace = defaultStencilOp;
	depthStencilDesc.BackFace = defaultStencilOp;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc;
	ZeroMemory(&pipelineStateDesc, sizeof(pipelineStateDesc));

	pipelineStateDesc.InputLayout = { inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };
	pipelineStateDesc.pRootSignature = rootSignature.Get();
	pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	
	pipelineStateDesc.RasterizerState = rasterizerDesc;
	pipelineStateDesc.BlendState = blendDesc;
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.SampleMask = UINT_MAX;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pipelineState;
	if (FAILED(device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()))))
	{
		throw(runtime_error{ "Error creating pipeline state." });
	}
	return pipelineState;
}

void Demo::createViewport() {
	RECT rect;
	if (!GetClientRect(window->getHandle(), &rect))
	{
		throw(runtime_error{ "Error getting window size." });
	}

	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<FLOAT>(rect.right - rect.left);
	viewport.Height = static_cast<FLOAT>(rect.bottom - rect.top);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void Demo::createScissorRect() {
	RECT rect;
	if (!GetClientRect(window->getHandle(), &rect))
	{
		throw(runtime_error{ "Error getting window size." });
	}

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = rect.right - rect.left;
	scissorRect.bottom = rect.bottom - rect.top;
}
