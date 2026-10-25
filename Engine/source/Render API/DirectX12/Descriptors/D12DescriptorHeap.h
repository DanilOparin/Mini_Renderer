#pragma once
#include <wrl.h>
namespace Engine {
	class D12DescriptorHeap : public Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
	{
	public:

		D12DescriptorHeap() = default;
		~D12DescriptorHeap();

		void InitializeRTV(ID3D12Device* pDevice, const unsigned int numdescriptors);
		void InitializeCBSRVUAV(ID3D12Device* pDevice, const unsigned int numdescriptors);
		void InitializeDepthHeap(ID3D12Device* pDevice, const unsigned int numdescriptors = 0);

		void Release();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const size_t idx = 0);

		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(const size_t idx = 0);
		void Alloc(
			D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle,
			D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);

		void* Ptr();

	private:
		
		UINT mIncrementSize = 0;
		unsigned int mSize = 0;
	};

}


