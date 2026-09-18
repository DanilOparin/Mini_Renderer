#include "pch.h"
#include "BufferUploader.h"


namespace Engine {



	BufferUploader::~BufferUploader()
	{
		Release();
	}

	void BufferUploader::Initialize(ID3D12Device* pDevice, const UINT memoryallocation)
	{

		mInternalBuffer.Initialize(pDevice, memoryallocation, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	
		mInternalMemory = mInternalBuffer.GetCPUMemory();
		mMaxSize = memoryallocation;

	}

	void BufferUploader::Upload(D12Resource* pDestination, void* pSourceData, const UINT size, D12CommandList* pCommandList, D12CommandQueue* pCommandQueue, D3D12_RESOURCE_STATES customState)
	{
		YT_ASSERT(size < mMaxSize);

		memcpy(mInternalMemory, pSourceData, size);

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pDestination->Get();
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		pCommandList->GFXCmd()->ResourceBarrier(1, &barrier);
		pCommandList->GFXCmd()->CopyBufferRegion(pDestination->Get(), 0, mInternalBuffer.Get(), 0, size);
		


		barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pDestination->Get();
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = (customState == D3D12_RESOURCE_STATE_COMMON ? D3D12_RESOURCE_STATE_COMMON : customState);

		pCommandList->GFXCmd()->ResourceBarrier(1, &barrier);

		pCommandList->GFXCmd()->Close();

		pCommandQueue->M_ExecuteCommandList(pCommandList->Get());

		while (pCommandQueue->GetFence()->GetCompletedValue() < pCommandQueue->M_GetCurrentFenceValue()) {

			_mm_pause();
		}


		pCommandList->ResetCommandList();
	}

	void BufferUploader::UploadTexture(ID3D12Device* pDevice, D12CommandList* pCommandList, D12CommandQueue* pCommandQueue, ID3D12Resource* pTexture, void* pixels, UINT width,UINT height)
	{
		UINT64 uploadSize = 0;

		

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT numRows;
		UINT64 rowSize;
		UINT64 uploadSize;

		pDevice->GetCopyableFootprints(
			&pTexture->GetDesc(),
			0,
			1,
			0,
			&footprint,
			&numRows,
			&rowSize,
			&uploadSize);
		
		D12Resource uploadBuffer;

		D3D12_HEAP_PROPERTIES heapProp = {};
		heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;

		D3D12_RESOURCE_DESC uploadDesc = {};

		uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadDesc.Width = uploadSize;
		uploadDesc.Height = 1;
		uploadDesc.DepthOrArraySize = 1;
		uploadDesc.MipLevels = 1;
		uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
		uploadDesc.SampleDesc = { 1,0 };
		uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		pDevice->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf()));

		D3D12_SUBRESOURCE_DATA textureData = {};

		textureData.pData = pixels;

		textureData.RowPitch = width * 4;

		textureData.SlicePitch = textureData.RowPitch * height;

		

		memcpy(mInternalMemory,pixels,width * height * 4);



		D3D12_RESOURCE_BARRIER barrier = {};

		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		barrier.Transition.pResource = pTexture;

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;

		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pCommandList->GFXCmd()->ResourceBarrier(1,&barrier);


		pCommandList->GFXCmd()->CopyTextureRegion();



		barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pDestination->Get();
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = (customState == D3D12_RESOURCE_STATE_COMMON ? D3D12_RESOURCE_STATE_COMMON : customState);

		pCommandList->GFXCmd()->ResourceBarrier(1, &barrier);

		pCommandList->GFXCmd()->Close();

		pCommandQueue->M_ExecuteCommandList(pCommandList->Get());

		while (pCommandQueue->GetFence()->GetCompletedValue() < pCommandQueue->M_GetCurrentFenceValue()) {

			_mm_pause();
		}


		pCommandList->ResetCommandList();
	}



	void BufferUploader::Release()
	{
		if (mInternalMemory) {
			mInternalMemory = nullptr;
		}
		mInternalBuffer.Release();
	}

}
