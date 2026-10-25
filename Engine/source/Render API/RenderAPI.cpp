
#include "pch.h"

#include "RenderAPI.h"



#include "DirectX12/DXGI/DXGIFactory.h"
#include "DirectX12/DXGI/DXGIAdapter.h"


#include "DirectX12/Debug/D12Debug.h"


#include "../Utilities/Utilities.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//extern stbi_uc* stbi_load(const char* filename, int* x, int* y, int* comp, int req_comp);



namespace Engine {
	using namespace Render;


	RenderAPI::~RenderAPI()
	{

		
		Release();
	}

	void RenderAPI::Initialize(HWND hwnd, const UINT width, const UINT height)
	{
		mWidth = width;
		mHeight = height;

		constexpr unsigned int shadowmapresolution = 2048;

		//this could be disabled during non-debug-builds
		D12Debug::Get().Enable();

		DXGIFactory factory;
		DXGIAdapter adapter = factory.GetAdapter();

		/* CONSOLE OUTPUT FOR DEBUGGING */
		{
			DXGI_ADAPTER_DESC desc;
			adapter->GetDesc(&desc);
			PRINT_W_N("Selected device " << desc.Description);
		}
		/* END DEBUGGING OUTPUT */

		/* ESSENTIALS */
		{
			mDevice.Init(adapter.Get());
			mDevice->SetName(L"Main virtual device");

			mCommandQueue.Initialize(mDevice.Get());
			mCommandList.Initialize(mDevice.Get());

			mSwapChain.Initialize(mDevice.Get(), factory.Get(), mCommandQueue.Get(), hwnd, mWidth, mHeight);
			mBufferUploader.Initialize(mDevice.Get(), MBs(32));
			mTextureUploader.Initialize(mDevice.Get(), MBs(32));

		}

		

		/* LOAD OUR FBX MODELS */
		{
			std::vector<Vertex> vertices;
			std::vector<UINT32> indices;

			mModelLoader.LoadFBXModel("models/Cube.fbx", vertices, indices, mMeshes);


			mVertexBuffer.Initialize(mDevice.Get(), MBs(64), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			mVertexBuffer.Get()->SetName(L"Vertex buffer");

			mBufferUploader.Upload((D12Resource*)mVertexBuffer.GetAddressOf(), vertices.data(), sizeof(Vertex) * vertices.size(),
				(D12CommandList*)mCommandList.GetAddressOf(), (D12CommandQueue*)mCommandQueue.GetAddressOf(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

			mVBView.BufferLocation = mVertexBuffer.Get()->GetGPUVirtualAddress();
			mVBView.StrideInBytes = sizeof(Vertex);
			mVBView.SizeInBytes = MBs(64);

			mIndexBuffer.Initialize(mDevice.Get(), MBs(64), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			mIndexBuffer->SetName(L"Index buffer");

			mBufferUploader.Upload((D12Resource*)mIndexBuffer.GetAddressOf(), indices.data(), sizeof(UINT32) * indices.size(),
				(D12CommandList*)mCommandList.GetAddressOf(), (D12CommandQueue*)mCommandQueue.GetAddressOf());
			mIBView.BufferLocation = mIndexBuffer.Get()->GetGPUVirtualAddress();
			mIBView.Format = DXGI_FORMAT_R32_UINT;
			mIBView.SizeInBytes = MBs(64);
		}
		

		/* DESCRIPTOR HEAPS */
		{
			mDeferredRenderTargetsDescHeap.InitializeRTV(mDevice.Get(), 4); //The deferred and forward pipeline: 4 render targets(3 - deffer 1 - forward)
			mDeferredRenderTargetsDescHeap->SetName(L"Render Target Descriptor heap");
			mForwardRenderTargetsDescHeap.InitializeRTV(mDevice.Get(), 1); //The deferred and forward pipeline: 4 render targets(3 - deffer 1 - forward)
			mForwardRenderTargetsDescHeap->SetName(L"Forward Render Target Descriptor heap");
			mCBSRVUAVDescHeap.InitializeCBSRVUAV(mDevice.Get(), 7); //should hold our shadowmap stuff in the future, so should have another name eventually and bigger size
			mCBSRVUAVDescHeap->SetName(L"Main CB, SRV, UAV Descriptor heap");

			mDepthDescHeap.InitializeDepthHeap(mDevice.Get(), 2);
			mDepthDescHeap->SetName(L"Depth descriptor heap");

		}

		//initialize imgui
		{
			// Setup Dear ImGui context
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			

			ImGui::StyleColorsDark();
			ImGui::GetStyle().ScaleAllSizes(1.5f);

			ImGui_ImplWin32_Init(hwnd);

			ImGui_ImplDX12_InitInfo init_info = {};
			init_info.Device = mDevice.Get();
			init_info.CommandQueue = mCommandQueue.Get();
			init_info.NumFramesInFlight = 2;
			init_info.RTVFormat = DXGI_FORMAT_R32G32B32A32_FLOAT; 
			init_info.UserData = mCBSRVUAVDescHeap.Ptr();
			init_info.SrvDescriptorHeap = mCBSRVUAVDescHeap.Get();
			init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
				auto* heap = static_cast<D12DescriptorHeap*>(info->UserData);
				return heap->Alloc(out_cpu_handle, out_gpu_handle); };
			init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return ; };

			ImGui_ImplDX12_Init(&init_info);

		}
		
		/* deferred pipeline stuff */
		{
			mDeferredPipeline.InitializeDeferred(mDevice.Get());
			mDeferredPipeline->SetName(L"Deferred pipeline PSO");

			mDeferredPixelPipeline.InitializePixelProces(mDevice.Get(), L"shaders/postfx/deferredlights/PS.hlsl");
			mDeferredPixelPipeline->SetName(L"Deferred pixel pipeline PSO");

			D3D12_RESOURCE_BARRIER barrier = {};
			{
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.Subresource = 0;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}
			const unsigned int rendertargets = 3;

			/* create our deferred buffers (G-BUFFER COMPONENTS) */
			for (int i = 0; i < rendertargets; i++) {

				mDeferredOutputTextures[i].InitializeAsRenderTarget(mDevice.Get(), width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);

				barrier.Transition.pResource = mDeferredOutputTextures[i].Get();
				mCommandList.GFXCmd()->ResourceBarrier(1, &barrier);

				mDevice->CreateRenderTargetView(mDeferredOutputTextures[i].Get(), nullptr, mDeferredRenderTargetsDescHeap.GetCPUHandle(i));
				mDevice->CreateShaderResourceView(mDeferredOutputTextures[i].Get(), &mDeferredOutputTextures[i].GetSRV(), mCBSRVUAVDescHeap.GetCPUHandle(i));

				std::wstring name = L"G-BUFFER rendertarget #";
				name.append(std::to_wstring(i + 1));
				mDeferredOutputTextures[i]->SetName(name.c_str());
			}
		}

		//forward pipeline stuff
		{
			mForwardPipeline.InitializeForward(mDevice.Get());
			mForwardPipeline->SetName(L"Forward pipeline PSO");

			mForwardOutputTextures[0].InitializeAsRenderTarget(mDevice.Get(), width, height, DXGI_FORMAT_R32G32B32A32_FLOAT);

			D3D12_RESOURCE_BARRIER barrier = {};
			{
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.Subresource = 0;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			}

			barrier.Transition.pResource = mForwardOutputTextures[0].Get();
			mCommandList.GFXCmd()->ResourceBarrier(1, &barrier);

			mDevice->CreateRenderTargetView(mForwardOutputTextures[0].Get(), nullptr, mForwardRenderTargetsDescHeap.GetCPUHandle(0));
		

		}


		/* shadow map pipeline */

		{
			mShadowMapPipeline.InitializeShadowMapping(mDevice.Get());
			mShadowMapPipeline->SetName(L"Shadow map pipeline");

		}



		

		/* DEPTH BUFFERS */
		{
			/* regular */
			D3D12_SHADER_RESOURCE_VIEW_DESC texturedesc = {};
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};

			{
				mDepthBuffers[0].InitializeAsDepthBuffer(mDevice.Get(), mWidth, mHeight);
				mDepthBuffers[0]->SetName(L"G-BUFFER depth buffer");

				dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Texture2D.MipSlice = 0;
				dsvDesc.Flags = D3D12_DSV_FLAG_NONE;


				texturedesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				texturedesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				texturedesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

				texturedesc.Texture2D.MipLevels = 1;
				texturedesc.Texture2D.MostDetailedMip = 0;
				texturedesc.Texture2D.PlaneSlice = 0;
				texturedesc.Texture2D.ResourceMinLODClamp = 0.0f;


				mDevice->CreateDepthStencilView(mDepthBuffers[0].Get(), &dsvDesc, mDepthDescHeap.GetCPUHandle(0));
				mDevice->CreateShaderResourceView(mDepthBuffers[0].Get(), &texturedesc, mCBSRVUAVDescHeap.GetCPUHandle(3));
			}
			


			/* shadow map depth buffer */

			{
				mDepthBuffers[1].InitializeAsDepthBuffer(mDevice.Get(), shadowmapresolution, shadowmapresolution, DXGI_FORMAT_D32_FLOAT);
				mDepthBuffers[1]->SetName(L"ShadowMap Depthbuffer");

				dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Texture2D.MipSlice = 0;
				dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

				texturedesc.Format = DXGI_FORMAT_R32_FLOAT;
				texturedesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				texturedesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

				texturedesc.Texture2D.MipLevels = 1;
				texturedesc.Texture2D.MostDetailedMip = 0;
				texturedesc.Texture2D.PlaneSlice = 0;
				texturedesc.Texture2D.ResourceMinLODClamp = 0.0f;

				mDevice->CreateDepthStencilView(mDepthBuffers[1].Get(), &dsvDesc, mDepthDescHeap.GetCPUHandle(1));
				mDevice->CreateShaderResourceView(mDepthBuffers[1].Get(), &texturedesc, mCBSRVUAVDescHeap.GetCPUHandle(4));
			}
			

		}
		
		/* viewport and scissor rect */
		{

			/* regular viewport */
			mViewport[0].TopLeftX = 0;
			mViewport[0].TopLeftY = 0;
			mViewport[0].Width = mWidth;
			mViewport[0].Height = mHeight;
			mViewport[0].MinDepth = 0.0f;
			mViewport[0].MaxDepth = 1.0f;

			mSRRect[0].left = 0;
			mSRRect[0].right = mViewport[0].Width;
			mSRRect[0].top = 0;
			mSRRect[0].bottom = mViewport[0].Height;


			/* shadowmap viewport */
			mViewport[1].TopLeftX = 0;
			mViewport[1].TopLeftY = 0;
			mViewport[1].Width = shadowmapresolution;
			mViewport[1].Height = shadowmapresolution;
			mViewport[1].MinDepth = 0.0f;
			mViewport[1].MaxDepth = 1.0f;

			mSRRect[1].left = 0;
			mSRRect[1].right = mViewport[1].Width;
			mSRRect[1].top = 0;
			mSRRect[1].bottom = mViewport[1].Height;

		}


		/* camera matrix stuff */
		{
			DirectX::XMMATRIX viewMatrix;
			viewMatrix = DirectX::XMMatrixLookAtLH({ 0.0f, 5.5f,-19.0f,0.0f }, { 0.0f,0.0f,0.0f,0.0f }, { 0.0f,1.0f,0.0f,0.0f });
			//DirectX::XMMatrixLookToLH({VEC3 pos},{VEC3 normalizedForward}, {VEC3 normalized updirection});

			DirectX::XMMATRIX projectionMatrix;
			projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(1.2217304764f, 16.0f / 9.0f, 1.0f, 50.0f);

			mViewProjectionMatrix = viewMatrix * projectionMatrix;

			mCBPassData.Initialize(mDevice.Get(), Utils::CalculateConstantbufferAlignment(sizeof(PassData)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
			mCBPassData->SetName(L"PassData CB");
		}


		//material allocations

		{
			

			mMaterialBuffers.emplace_back(D12Resource());
			mMaterialBuffers[0].Initialize(mDevice.Get(), Utils::CalculateConstantbufferAlignment(sizeof(MaterialCelShader)), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
			mMaterialBuffers[0]->SetName(L"Material_1");

			MaterialCelShader material;
			material.diffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };

			mBufferUploader.Upload((D12Resource*)mMaterialBuffers[0].GetAddressOf(), &material, sizeof(MaterialCelShader),
				(D12CommandList*)mCommandList.GetAddressOf(), (D12CommandQueue*)mCommandQueue.GetAddressOf(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

			

		}

		mLights[0].position = { 0.0f,0.0f,0.0f };
		mLights[0].strength = 1.0f;
		mLights[0].direction = { 0.3f,-0.4f,0.5f };
		
		//Transform allocations
		{
			mObjTransforms.emplace_back(D12Resource());
			mObjTransforms[0].Initialize(mDevice.Get(), Utils::CalculateConstantbufferAlignment(sizeof(ObjectData)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
			mObjTransforms[0]->SetName(L"Transform 1 CB");

			ObjectData tempData;
			
			DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(4.0, 4.0f, 4.0f);
			DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationAxis({0.0f,90.0f,90.0f}, 3.14f);
			DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
			

			tempData.transform = scale * rotation * translation;
			mObjTransformsCPU.push_back(tempData);

			memcpy(mObjTransforms[0].GetCPUMemory(), &tempData, sizeof(ObjectData));

			

		}

		//Shadow transforms
		{

			/*
			mShadowTransforms.resize(mObjTransforms.size());


			for (int i = 0; i < mObjTransforms.size(); i++) {
				mShadowTransforms[i].Initialize(mDevice.Get(), Utils::CalculateConstantbufferAlignment(sizeof(ObjectData)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
				std::wstring name = L"Shadow transform buffer ";
				name.append(std::to_wstring(i));

				mShadowTransforms[i]->SetName(name.c_str());

				ObjectData tempData;

				memcpy(mShadowTransforms[i].GetCPUMemory(), &tempData, sizeof(ObjectData));


			}
			*/


		}

		{

			
			int x, y, comp;
			const stbi_uc* texture = stbi_load("Textures/Brick.jpg", &x, &y, &comp, 4);
			std::cout << "x:" << x << "y" << y<<std::endl;

			mTextureBuffers.emplace_back(D12Resource());
			mTextureBuffers[0].InitializeAsTexture(mDevice.Get(), x, y, DXGI_FORMAT_R8G8B8A8_UNORM);
			mTextureBuffers[0]->SetName(L"BrickTexture");

			mTextureUploader.UploadTexture(mDevice.Get(), (D12CommandList*)mCommandList.GetAddressOf(), (D12CommandQueue*)mCommandQueue.GetAddressOf(), (D12Resource*)mTextureBuffers[0].GetAddressOf(), (void*)texture, x, y);

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MipLevels = 1;
			mDevice->CreateShaderResourceView(mTextureBuffers[0].Get(), &srvDesc, mCBSRVUAVDescHeap.GetCPUHandle(5));
			auto desc =
				mTextureBuffers[0]->GetDesc();

			std::cout << desc.Dimension << std::endl;
			stbi_image_free((void*)texture);
		}

		DefferedRendering = true;
		ForwardRendering = false;

	}





	void RenderAPI::UpdateDraw(const float ts)
	{
		
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		

		ImGui::Begin("Hello, world!");
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Checkbox("Forward Rendering", &ForwardRendering);
		ImGui::Checkbox("Deffered Rendering", &DefferedRendering);
		ImGui::End();

		ImGui::Render();

		Light currentFrameLights = mLights[0];
		PassData passData;



		//Simulation
		{
		
			float increment = 0.05f * ts;

			if (mLightCycle >= 1.0f) {
				mLightCycle = 0.0f;
			}
		
			mLightCycle += increment;


			float clampedCycle = (mLightCycle * 2.0f) - 1.0f; // this will set it between -1 and 1 (with 0.0 = midday)
			float maxrotation = 3.14;// / 2.0f;
			float actualrotation = maxrotation * clampedCycle;

			DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationNormal({ 0.0f,0.0f,1.0f,0.0f }, actualrotation);

			DirectX::XMVECTOR frameLightDirection = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&mLights[0].direction), rotationMatrix);

			//DirectX::XMStoreFloat3(&currentFrameLights.direction, frameLightDirection);

			/* Shadowmap computations */
			{
				float shadowmapradius = 50.0f;

				DirectX::XMVECTOR scenecenter = { 0.0f,0.0f,0.0f };
				DirectX::XMVECTOR lightposition = DirectX::XMVectorScale(DirectX::XMLoadFloat3(&currentFrameLights.direction),-shadowmapradius);

				DirectX::XMMATRIX viewlights = DirectX::XMMatrixLookAtLH(lightposition, scenecenter, { 0.0f,1.0f,0.0f,0.0f });

				DirectX::XMMATRIX projectionlight = DirectX::XMMatrixOrthographicLH(20, 20, 0.5f, 200.0f);

				passData.lightviewproject = viewlights * projectionlight;

			}

		}

		//Update buffers
		{
			passData.viewproject = mViewProjectionMatrix;
			passData.scenelight = currentFrameLights;

			memcpy(mCBPassData.GetCPUMemory(), &passData, sizeof(PassData));
		}

		ID3D12DescriptorHeap* descriptorheaps[1] = { mCBSRVUAVDescHeap.Get() };
		mCommandList.GFXCmd()->SetDescriptorHeaps(1, descriptorheaps);

		////Draw to shadowmap
		//{

		//	mCommandList.GFXCmd()->RSSetViewports(1, &mViewport[1]);
		//	mCommandList.GFXCmd()->RSSetScissorRects(1, &mSRRect[1]);


		//	mCommandList.GFXCmd()->SetGraphicsRootSignature(mShadowMapPipeline.GetRS());
		//	mCommandList.GFXCmd()->SetPipelineState(mShadowMapPipeline.Get());
		//	mCommandList.GFXCmd()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//	mCommandList.GFXCmd()->IASetVertexBuffers(0, 1, &mVBView);
		//	mCommandList.GFXCmd()->IASetIndexBuffer(&mIBView);

		//	/* clear depth buffer for the shadowmap */
		//	{

		//		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDepthDescHeap.GetCPUHandle(1); //shadow map depth buffer here
		//	
		//		mCommandList.GFXCmd()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, 0);
		//		mCommandList.GFXCmd()->OMSetRenderTargets(0, nullptr, false, &dsvHandle);				
		//	}

		//	mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(0, mCBPassData.Get()->GetGPUVirtualAddress());
		//	//skeleton
		//	{
		//		mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(1, mObjTransforms[0].Get()->GetGPUVirtualAddress());
		//		mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(2, mMaterialBuffers[0].Get()->GetGPUVirtualAddress());

		//		Render::MeshDrawData* drawdata = &mMeshes[0];

		//		mCommandList.GFXCmd()->DrawIndexedInstanced(drawdata->indexcount, 1, drawdata->indexoffset, drawdata->vertexoffset, 0);
		//	}
		



		//}

		mRenderMode = DefferedRendering ? Engine::RenderAPI::RenderMode::Deffered : Engine::RenderAPI::RenderMode::Forward;

		switch (mRenderMode)
		{
		case Engine::RenderAPI::RenderMode::Deffered:
			ForwardRendering = false;
			break;
		case Engine::RenderAPI::RenderMode::Forward:
			DefferedRendering = false;
			break;
		default:
			break;
		}


		if(ForwardRendering)
		{
			mCommandList.GFXCmd()->RSSetViewports(1, &mViewport[0]);
			mCommandList.GFXCmd()->RSSetScissorRects(1, &mSRRect[0]);

			mCommandList.GFXCmd()->SetGraphicsRootSignature(mForwardPipeline.GetRS());
			mCommandList.GFXCmd()->SetPipelineState(mForwardPipeline.Get());
			mCommandList.GFXCmd()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mCommandList.GFXCmd()->IASetVertexBuffers(0, 1, &mVBView);
			mCommandList.GFXCmd()->IASetIndexBuffer(&mIBView);

			const float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle[1] = { mForwardRenderTargetsDescHeap.GetCPUHandle(0) };

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDepthDescHeap.GetCPUHandle(0);

			mCommandList.GFXCmd()->ClearRenderTargetView(mForwardRenderTargetsDescHeap.GetCPUHandle(0), clearColor, 0, 0);
			mCommandList.GFXCmd()->ClearDepthStencilView(mDepthDescHeap.GetCPUHandle(0), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, 0);

			mCommandList.GFXCmd()->OMSetRenderTargets(1, rtvHandle, false, &dsvHandle);

			mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(0, mCBPassData.Get()->GetGPUVirtualAddress());
			mCommandList.GFXCmd()->SetGraphicsRootDescriptorTable(3, mCBSRVUAVDescHeap.GetGPUHandle(0));

			for (Render::MeshDrawData& mesh: mMeshes)
			{
				mCommandList.GFXCmd()->DrawIndexedInstanced(mesh.indexcount, 1, mesh.indexoffset, mesh.vertexoffset, 0);
			}

		}
		else
		{

		}
		{
			mCommandList.GFXCmd()->RSSetViewports(1, &mViewport[0]);
			mCommandList.GFXCmd()->RSSetScissorRects(1, &mSRRect[0]);

			
			mCommandList.GFXCmd()->SetGraphicsRootSignature(mDeferredPipeline.GetRS());
			mCommandList.GFXCmd()->SetPipelineState(mDeferredPipeline.Get());
			mCommandList.GFXCmd()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mCommandList.GFXCmd()->IASetVertexBuffers(0, 1, &mVBView);
			mCommandList.GFXCmd()->IASetIndexBuffer(&mIBView);
			


			const float clearColor[] = { 0.0f,0.0f,0.0f,0.0f };

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle[3] = {	mDeferredRenderTargetsDescHeap.GetCPUHandle(0),
															mDeferredRenderTargetsDescHeap.GetCPUHandle(1),
															mDeferredRenderTargetsDescHeap.GetCPUHandle(2) };

			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDepthDescHeap.GetCPUHandle(0);

			mCommandList.GFXCmd()->ClearRenderTargetView(rtvHandle[0], clearColor, 0, 0);
			mCommandList.GFXCmd()->ClearRenderTargetView(rtvHandle[1], clearColor, 0, 0);
			mCommandList.GFXCmd()->ClearRenderTargetView(rtvHandle[2], clearColor, 0, 0);

			mCommandList.GFXCmd()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, 0);
			mCommandList.GFXCmd()->OMSetRenderTargets(3, rtvHandle, false, &dsvHandle);

			mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(0, mCBPassData.Get()->GetGPUVirtualAddress());
			mCommandList.GFXCmd()->SetGraphicsRootDescriptorTable(3, mCBSRVUAVDescHeap.GetGPUHandle(0));



			
			{

				mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(1, mObjTransforms[0].Get()->GetGPUVirtualAddress());
				mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(2, mMaterialBuffers[0].Get()->GetGPUVirtualAddress());

				Render::MeshDrawData* drawdata = &mMeshes[0];

				mCommandList.GFXCmd()->DrawIndexedInstanced(drawdata->indexcount, 1, drawdata->indexoffset, drawdata->vertexoffset, 0);

			}

			

		
			D3D12_RESOURCE_BARRIER barrier = {};

		

			/* Set our swapchain backbuffer in an rendertarget state*/
			{
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.pResource = mSwapChain.GetCurrentRenderTarget();
				barrier.Transition.Subresource = 0;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

				mCommandList.GFXCmd()->ResourceBarrier(1, &barrier);
			}

			mCommandList.GFXCmd()->SetGraphicsRootSignature(mDeferredPixelPipeline.GetRS());
			mCommandList.GFXCmd()->SetPipelineState(mDeferredPixelPipeline.Get());
			mCommandList.GFXCmd()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mCommandList.GFXCmd()->IASetVertexBuffers(0, 1, &mVBView);
			mCommandList.GFXCmd()->IASetIndexBuffer(&mIBView);


			/* clear the swapchain and depth buffer and bind it as output */
			{
				const float clearColor[] = { 0.0f,0.0f,0.0f,1.0f };
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = mSwapChain.GetCurrentRTVHandle();
			
				mCommandList.GFXCmd()->ClearRenderTargetView(rtvHandle, clearColor, 0, 0);
				mCommandList.GFXCmd()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
				
			}

			mCommandList.GFXCmd()->SetGraphicsRootConstantBufferView(0, mCBPassData.Get()->GetGPUVirtualAddress());
			mCommandList.GFXCmd()->SetGraphicsRootDescriptorTable(3, mCBSRVUAVDescHeap.GetGPUHandle(0));

			unsigned int size[2] = { 1280,720 };

			


			mCommandList.GFXCmd()->DrawIndexedInstanced(3, 1, 0, 0, 0);

			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.GFXCmd());
			/* Transition the swapchain backbuffer into a present state */
			{
				barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition.pResource = mSwapChain.GetCurrentRenderTarget();
				barrier.Transition.Subresource = 0;
				barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

				mCommandList.GFXCmd()->ResourceBarrier(1, &barrier);
			}

			
			/* Close command list and execute + present */

			{
				mCommandList.GFXCmd()->Close();
				mCommandQueue.M_ExecuteCommandList(mCommandList.Get());

				mSwapChain.Present();

				while (mCommandQueue.GetFence()->GetCompletedValue() < mCommandQueue.M_GetCurrentFenceValue()) {

					_mm_pause();
				}

				mCommandList.ResetCommandList();
			}

		}
		
	}

	void RenderAPI::Release()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		
		/* Fill out this stuff */

		mCommandQueue.FlushQueue();

		for (int i = 0; i < mMaterialBuffers.size(); i++) {
			mMaterialBuffers[i].Release();
		}



		for (int i = 0; i < mObjTransforms.size(); i++) {
			mObjTransforms[i].Release();
		}

		for (D12Resource& resource: mTextureBuffers)
		{
			resource.Release();
		}

		for(D12Resource& resource: mDeferredOutputTextures)
		{
			resource.Release();
		}

		mVertexBuffer.Release();
		mIndexBuffer.Release();
		mCBPassData.Release();

		mDeferredPipeline.Release();
		mDeferredPixelPipeline.Release();
		mShadowMapPipeline.Release();
		mForwardPipeline.Release();


		mDeferredRenderTargetsDescHeap.Release();
		mCBSRVUAVDescHeap.Release();
		mDepthDescHeap.Release();


		mDepthBuffers[0].Release();
		mDepthBuffers[1].Release();

		mBufferUploader.Release();
		mTextureUploader.Release();
	
		mSwapChain.Release();
		
		mCommandList.Release();

		mCommandQueue.Release();


		if (mDevice.Get()) {

			mDevice.Reset();

		}
	}

}




