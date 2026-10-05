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

	

	void BufferUploader::UploadTexture(ID3D12Device* pDevice, D12CommandList* pCommandList, D12CommandQueue* pCommandQueue, D12Resource* pTexture, void* pixels, UINT width,UINT height)
	{


		

		std::cout
			<< "UploadSize = "
			<< 512 * 512 * 4
			<< std::endl;

		std::cout
			<< "MaxSize = "
			<< mMaxSize
			<< std::endl;

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		UINT numRows;
		UINT64 rowSize;
		UINT64 uploadSize;

		D3D12_RESOURCE_DESC resDesc = {};
		resDesc = pTexture->Get()->GetDesc();
		pDevice->GetCopyableFootprints(
			&resDesc,
			0,
			1,
			0,
			&footprint,
			&numRows,
			&rowSize,
			&uploadSize);
		
		std::cout
			<< "Width: "
			<< footprint.Footprint.Width
			<< std::endl;

		std::cout
			<< "Height: "
			<< footprint.Footprint.Height
			<< std::endl;

		std::cout
			<< "RowPitch: "
			<< footprint.Footprint.RowPitch
			<< std::endl;
	
		
		for (UINT row = 0; row < height; row++)
		{
			memcpy(
				(char*)(mInternalMemory) + row * footprint.Footprint.RowPitch,
				(char*)(pixels) + row * width * 4,
				width * 4);
		}

		D3D12_TEXTURE_COPY_LOCATION dstCP = {};
		dstCP.pResource = pTexture->Get();
		dstCP.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dstCP.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION srcCP = {};
		srcCP.PlacedFootprint = footprint;
		srcCP.pResource = mInternalBuffer.Get();
		srcCP.SubresourceIndex = 0;
		srcCP.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

		D3D12_RESOURCE_BARRIER barrier = {};

		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

		barrier.Transition.pResource = pTexture->Get();

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;

		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pCommandList->GFXCmd()->ResourceBarrier(1,&barrier);


		pCommandList->GFXCmd()->CopyTextureRegion(&dstCP, 0, 0, 0, &srcCP, nullptr);



		barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = pTexture->Get();
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

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
