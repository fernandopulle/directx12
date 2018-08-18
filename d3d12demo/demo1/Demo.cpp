#include <stdexcept>
#include <d3dcompiler.h>
#include "Demo.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

Demo::Demo(UINT bufferCount, string name, LONG width, LONG height) :DGraphics{ bufferCount, name, width, height } {

	auto lambda = [this](WPARAM wparam) {
		switch (wparam)
		{
		default:
			break;
		}
	};
	/* this is our keypress callback function. */
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
	
	// reset the command list. by resetting the command list we are putting it into
	// a recording state so we can start recording commands into the command allocator.
	// the command allocator that we reference here may have multiple command lists
	// associated with it, but only one can be recording at any time. Make sure
	// that any other command lists associated to this command allocator are in
	// the closed state (not recording).
	// Here you will pass an initial pipeline state object as the second parameter,
	// but in this tutorial we are only clearing the rtv, and do not actually need
	// anything but an initial default pipeline, which is what we get by setting
	// the second parameter to NULL
	if (FAILED(commandList->Reset(commandAllocator.Get(), nullptr)))
	{
		throw(runtime_error{ "Error resetting command list." });
	}

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

	/* --------------------------------------------------------------------------------------*/

	static UINT descriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv(descHeapRtv->GetCPUDescriptorHandleForHeapStart());
	descHandleRtv.ptr += frameIndex * descriptorSize;

	D3D12_CPU_DESCRIPTOR_HANDLE descHandleDepthStencil(descHeapDepthStencil->GetCPUDescriptorHandleForHeapStart());
	

	commandList->OMSetRenderTargets(1, &descHandleRtv, FALSE, &descHandleDepthStencil);

	// Clear the render target by using the ClearRenderTargetView command
	static float clearColor[]{ 0.1f, 0.1f, 0.1f, 1.0f };
	commandList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(descHeapDepthStencil->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	ZeroMemory(&barrierDesc, sizeof(barrierDesc));
	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = currBuffer;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &barrierDesc);

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

