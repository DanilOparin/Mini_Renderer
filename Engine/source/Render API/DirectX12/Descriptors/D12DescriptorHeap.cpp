#include "pch.h"
#include "D12DescriptorHeap.h"
namespace Engine {
	D12DescriptorHeap::~D12DescriptorHeap()
	{
		Release();
	}
	void D12DescriptorHeap::InitializeRTV(ID3D12Device* pDevice, const unsigned int numdescriptors)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = numdescriptors;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		YT_EVAL_HR(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(GetAddressOf())), "Error creating descriptor heap");

		mIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		mSize = numdescriptors;
	}
	void D12DescriptorHeap::InitializeCBSRVUAV(ID3D12Device* pDevice, const unsigned int numdescriptors)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = numdescriptors;
		desc.NodeMask = 0;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		YT_EVAL_HR(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(GetAddressOf())), "Error creating descriptor heap");

		mIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		mSize = numdescriptors;
	}
	void D12DescriptorHeap::InitializeDepthHeap(ID3D12Device* pDevice, const unsigned int numdescriptors)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors = numdescriptors;
		desc.NodeMask = 0;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		YT_EVAL_HR(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(GetAddressOf())), "Error creating descriptor heap");

		mIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		mSize = numdescriptors;
	}

	void D12DescriptorHeap::Release()
	{

		if (Get()) {

			Reset();
		}
		mIncrementSize = 0;
		mSize = 0;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE D12DescriptorHeap::GetCPUHandle(const size_t idx)
	{
		assert(idx < mSize);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = Get()->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += idx * (size_t)mIncrementSize;

		return handle;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE D12DescriptorHeap::GetGPUHandle(const size_t idx)
	{
		assert(idx < mSize);

		D3D12_GPU_DESCRIPTOR_HANDLE handle = Get()->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += (UINT64)idx * (UINT64)mIncrementSize;

		return handle;
	}

	void D12DescriptorHeap::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
	{

		out_cpu_desc_handle->ptr = Get()->GetCPUDescriptorHandleForHeapStart().ptr + ((Get()->GetDesc().NumDescriptors - 1) * mIncrementSize);
		out_gpu_desc_handle->ptr = Get()->GetGPUDescriptorHandleForHeapStart().ptr + ((Get()->GetDesc().NumDescriptors - 1) * mIncrementSize);
	}

	void* D12DescriptorHeap::Ptr()
	{
		return (void*)this;
	}
	/*void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
	{
		int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
		int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
		IM_ASSERT(cpu_idx == gpu_idx);
		FreeIndices.push_back(cpu_idx);
	}*/
}