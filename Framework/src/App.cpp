#include "App.h"


namespace {
	const auto ClassName = TEXT("SampleWindowClass");
	struct Vertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT4 Color;
	};
}

template<typename T>
void SafeRelease(T*& ptr)
{
	if (ptr != nullptr)
	{
		ptr->Release();
			ptr = nullptr;
	}
}

App::App(uint32_t width, uint32_t height)
	: m_hInst (nullptr)
	, m_hWnd (nullptr)
	, m_Width (width)
	, m_Height (height)
	, m_pDevice (nullptr)
	, m_pQueue (nullptr)
	, m_pSwapChain (nullptr)
	, m_pCmdList (nullptr)
	, m_pHeapRTV (nullptr)
	, m_pFence (nullptr)
	, m_FrameIndex (0)
{
	for (auto i = 0u; i < FrameCount; ++i)
	{
		m_pColorBuffer[i] = nullptr;
		m_pCmdAllocator[i] = nullptr;
		m_FenceCounter[i] = 0;
	}
}

App::~App()
{}

void App::Run()
{
	if (InitApp())
	{
		MainLoop();
	}
	TermApp();
}

bool App::InitApp()
{
	if (!InitWnd())
	{
		return false;
	}
	if (!InitD3D())
	{
		return false;
	}
	if (!OnInit())
	{
		return false;
	}
	return true;
}

bool App::InitWnd()
{
	auto hInst = GetModuleHandle(nullptr);
	if (hInst == nullptr)
	{
		return false;
	}

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hIcon = LoadIcon(hInst, IDI_APPLICATION);
	wc.hCursor = LoadCursor(hInst, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = ClassName;
	wc.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
	if (!RegisterClassEx(&wc))
	{
		return false;
	}
	m_hInst = hInst;
	RECT rc = {};
	rc.right = static_cast<LONG>(m_Width);
	rc.bottom = static_cast<LONG>(m_Height);
	auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
	AdjustWindowRect(&rc, style, FALSE);
	m_hWnd = CreateWindowEx(
		0,
		ClassName,
		TEXT("Sample"),
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		m_hInst,
		nullptr);
	if (m_hWnd == nullptr)
	{
		return false;
	}
	ShowWindow(m_hWnd, SW_SHOWNORMAL);
	UpdateWindow(m_hWnd);
	SetFocus(m_hWnd);
	return true;
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_DESTROY:
	{ PostQuitMessage(0); }
	break;
	default:
	{}
	break;
	}
	return DefWindowProc(hWnd, msg, wp, lp);
}

void App::MainLoop()
{
	MSG msg = {};
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == TRUE)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}
}

void App::TermApp()
{
	TermD3D();
	TermWnd();
}

void App::TermWnd()
{
	if (m_hInst != nullptr)
	{
		UnregisterClass(ClassName, m_hInst);
	}
	m_hInst = nullptr;
	m_hWnd = nullptr;
}

bool App::InitD3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debug;
		auto hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));
		if (SUCCEEDED(hr))
		{
			debug->EnableDebugLayer();
		}
	}
#endif
	auto hr = D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(m_pDevice.GetAddressOf()));
	if (FAILED(hr))
	{
		return false;
	}
	{
		D3D12_COMMAND_QUEUE_DESC desc = {};
		desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		desc.NodeMask = 0;
		hr = m_pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pQueue));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		ComPtr<IDXGIFactory4> pFactory = nullptr;
		hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
		if (FAILED(hr))
		{
			return false;
		}
		DXGI_SWAP_CHAIN_DESC desc = {};
		desc.BufferDesc.Width = m_Width;
		desc.BufferDesc.Height = m_Height;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = FrameCount;
		desc.OutputWindow = m_hWnd;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		ComPtr<IDXGISwapChain> pSwapChain;
		hr = pFactory->CreateSwapChain(m_pQueue.Get(), &desc, &pSwapChain);
		if (FAILED(hr))
		{
			return false;
		}
		hr = pSwapChain->QueryInterface(IID_PPV_ARGS(&m_pSwapChain));
		if (FAILED(hr))
		{
			return false;
		}
		m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
		pFactory.Reset();
		pSwapChain.Reset();
	}
	{
		for (auto i = 0u; i < FrameCount; ++i)
		{
			hr = m_pDevice->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(&m_pCmdAllocator[i]));
			if (FAILED(hr))
			{
				return false;
			}
		}
	}
	{
		hr = m_pDevice->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			m_pCmdAllocator[m_FrameIndex].Get(),
			nullptr,
			IID_PPV_ARGS(&m_pCmdList));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = FrameCount;
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;
		hr = m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pHeapRTV));
		if (FAILED(hr))
		{
			return false;
		}
		auto handle = m_pHeapRTV->GetCPUDescriptorHandleForHeapStart();
		auto incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (auto i = 0u; i < FrameCount; ++i)
		{
			hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pColorBuffer[i]));
			if (FAILED(hr))
			{
				return false;
			}
			D3D12_RENDER_TARGET_VIEW_DESC viewDesc = {};
			viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = 0;
			viewDesc.Texture2D.PlaneSlice = 0;
			m_pDevice->CreateRenderTargetView(m_pColorBuffer[i].Get(), &viewDesc, handle);
			m_HandleRTV[i] = handle;
			handle.ptr += incrementSize;
		}
	}
	{
		for (auto i = 0u; i < FrameCount; ++i)
		{
			m_FenceCounter[i] = 0;
		}
		hr = m_pDevice->CreateFence(
			m_FenceCounter[m_FrameIndex],
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&m_pFence));
		if (FAILED(hr))
		{
			return false;
		}
		m_FenceCounter[m_FrameIndex]++;
		m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_FenceEvent == nullptr)
		{
			return false;
		}
	}
	m_pCmdList->Close();
	return true;
}

bool App::OnInit()
{
	{
		Vertex vertices[] = {
			{DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)},
			{DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
			{DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)},
		};
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(vertices);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto hr = m_pDevice->CreateCommittedResource(
			&prop,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_pVB.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
		void* ptr = nullptr;
		hr = m_pVB->Map(0, nullptr, &ptr);
		if (FAILED(hr))
		{
			return false;
		}
		memcpy(ptr, vertices, sizeof(vertices));
		m_pVB->Unmap(0, nullptr);
		m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress();
		m_VBV.SizeInBytes = static_cast<UINT>(sizeof(vertices));
		m_VBV.StrideInBytes = static_cast<UINT>(sizeof(Vertex));
	}
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1 * FrameCount;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;
		auto hr = m_pDevice->CreateDescriptorHeap(
			&desc,
			IID_PPV_ARGS(m_pHeapCBV.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_HEAP_PROPERTIES prop = {};
		prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		prop.CreationNodeMask = 1;
		prop.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(Transform);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		auto incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		for (auto i = 0; i < FrameCount; ++i)
		{
			auto hr = m_pDevice->CreateCommittedResource(
				&prop,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
			if (FAILED(hr))
			{
				return false;
			}
			auto address = m_pCB[i]->GetGPUVirtualAddress();
			auto handleCPU = m_pHeapCBV->GetCPUDescriptorHandleForHeapStart();
			auto handleGPU = m_pHeapCBV->GetGPUDescriptorHandleForHeapStart();
			handleCPU.ptr += incrementSize * i;
			handleGPU.ptr += incrementSize * i;
			m_CBV[i].HandleCPU = handleCPU;
			m_CBV[i].HandleGPU = handleGPU;
			m_CBV[i].Desc.BufferLocation = address;
			m_CBV[i].Desc.SizeInBytes = sizeof(Transform);
			m_pDevice->CreateConstantBufferView(&m_CBV[i].Desc, handleCPU);
			hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_CBV[i].pBuffer));
			if (FAILED(hr))
			{
				return false;
			}
			auto eyePos = DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
			auto targetPos = DirectX::XMVectorZero();
			auto upward = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			auto fovY = DirectX::XMConvertToRadians(37.5f);
			auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);
			m_CBV[i].pBuffer->World = DirectX::XMMatrixIdentity();
			m_CBV[i].pBuffer->View = DirectX::XMMatrixLookAtRH(eyePos, targetPos, upward);
			m_CBV[i].pBuffer->Proj = DirectX::XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
		}
	}
	{
		auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		D3D12_ROOT_PARAMETER param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		param.Descriptor.ShaderRegister = 0;
		param.Descriptor.RegisterSpace = 0;
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = 1;
		desc.NumStaticSamplers = 0;
		desc.pParameters = &param;
		desc.pStaticSamplers = nullptr;
		desc.Flags = flag;
		ComPtr<ID3DBlob> pBlob;
		ComPtr<ID3DBlob> pErrorBlob;
		auto hr = D3D12SerializeRootSignature(
			&desc,
			D3D_ROOT_SIGNATURE_VERSION_1_0,
			pBlob.GetAddressOf(),
			pErrorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}
		hr = m_pDevice->CreateRootSignature(
			0,
			pBlob->GetBufferPointer(),
			pBlob->GetBufferSize(),
			IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		D3D12_INPUT_ELEMENT_DESC elements[2];
		elements[0].SemanticName = "POSITION";
		elements[0].SemanticIndex = 0;
		elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
		elements[0].InputSlot = 0;
		elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[0].InstanceDataStepRate = 0;
		elements[1].SemanticName = "COLOR";
		elements[1].SemanticIndex = 0;
		elements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		elements[1].InputSlot = 0;
		elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[1].InstanceDataStepRate = 0;
		D3D12_RASTERIZER_DESC descRS;
		descRS.FillMode = D3D12_FILL_MODE_SOLID;
		descRS.CullMode = D3D12_CULL_MODE_NONE;
		descRS.FrontCounterClockwise = FALSE;
		descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		descRS.DepthBiasClamp = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		descRS.DepthClipEnable = FALSE;
		descRS.MultisampleEnable = FALSE;
		descRS.AntialiasedLineEnable = FALSE;
		descRS.ForcedSampleCount = 0;
		descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
			FALSE, FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL
		};
		D3D12_BLEND_DESC descBS;
		descBS.AlphaToCoverageEnable = FALSE;
		descBS.IndependentBlendEnable = FALSE;
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			descBS.RenderTarget[i] = descRTBS;
		}
		ComPtr<ID3DBlob> pVSBlob;
		ComPtr<ID3DBlob> pPSBlob;
		auto hr = D3DReadFileToBlob(L"SimpleVS.cso", pVSBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}
		hr = D3DReadFileToBlob(L"SimplePS.cso", pPSBlob.GetAddressOf());
		if (FAILED(hr))
		{
			return false;
		}
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
		desc.InputLayout = { elements, _countof(elements) };
		desc.pRootSignature = m_pRootSignature.Get();
		desc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
		desc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
		desc.RasterizerState = descRS;
		desc.BlendState = descBS;
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		hr = m_pDevice->CreateGraphicsPipelineState(
			&desc,
			IID_PPV_ARGS(m_pPSO.GetAddressOf()));
		if (FAILED(hr))
		{
			return false;
		}
	}
	{
		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;
		m_Viewport.Width = static_cast<float>(m_Width);
		m_Viewport.Height = static_cast<float>(m_Height);
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;
		m_Scissor.left = 0;
		m_Scissor.right = m_Width;
		m_Scissor.top = 0;
		m_Scissor.bottom = m_Height;
	}
	return true;
}

void App::Render()
{
	{
		m_RotateAngle += 0.025f;
		m_CBV[m_FrameIndex].pBuffer->World = DirectX::XMMatrixRotationY(m_RotateAngle);
	}
	m_pCmdAllocator[m_FrameIndex]->Reset();
	m_pCmdList->Reset(m_pCmdAllocator[m_FrameIndex].Get(), nullptr);
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pCmdList->ResourceBarrier(1, &barrier);
	m_pCmdList->OMSetRenderTargets(1, &m_HandleRTV[m_FrameIndex], FALSE, nullptr);
	float clearColor[] = { 0.25f, 0.25f, 0.25f, 1.0f };
	m_pCmdList->ClearRenderTargetView(m_HandleRTV[m_FrameIndex], clearColor, 0, nullptr);
	{
		m_pCmdList->SetGraphicsRootSignature(m_pRootSignature.Get());
		m_pCmdList->SetDescriptorHeaps(1, m_pHeapCBV.GetAddressOf());
		m_pCmdList->SetGraphicsRootConstantBufferView(0, m_CBV[m_FrameIndex].Desc.BufferLocation);
		m_pCmdList->SetPipelineState(m_pPSO.Get());
		m_pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pCmdList->IASetVertexBuffers(0, 1, &m_VBV);
		m_pCmdList->RSSetViewports(1, &m_Viewport);
		m_pCmdList->RSSetScissorRects(1, &m_Scissor);
		m_pCmdList->DrawInstanced(3, 1, 0, 0);
	}
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_pColorBuffer[m_FrameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pCmdList->ResourceBarrier(1, &barrier);
	m_pCmdList->Close();
	ID3D12CommandList* ppCmdLists[] = { m_pCmdList.Get() };
	m_pQueue->ExecuteCommandLists(1, ppCmdLists);
	Present(1);
}

void App::Present(uint32_t interval)
{
	m_pSwapChain->Present(interval, 0);
	const auto currentValue = m_FenceCounter[m_FrameIndex];
	m_pQueue->Signal(m_pFence.Get(), currentValue);
	m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
	if (m_pFence->GetCompletedValue() < m_FenceCounter[m_FrameIndex])
	{
		m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);
		WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
	}
	m_FenceCounter[m_FrameIndex] = currentValue + 1;
}

void App::WaitGpu()
{
	assert(m_pQueue != nullptr);
	assert(m_pFence != nullptr);
	assert(m_FenceEvent != nullptr);
	m_pQueue->Signal(m_pFence.Get(), m_FenceCounter[m_FrameIndex]);
	m_pFence->SetEventOnCompletion(m_FenceCounter[m_FrameIndex], m_FenceEvent);
	WaitForSingleObjectEx(m_FenceEvent, INFINITE, FALSE);
	m_FenceCounter[m_FrameIndex]++;
}

void App::TermD3D()
{
	WaitGpu();
	if (m_FenceEvent != nullptr)
	{
		CloseHandle(m_FenceEvent);
		m_FenceEvent = nullptr;
	}
	m_pFence.Reset();
	m_pHeapRTV.Reset();
	for (auto i = 0u; i < FrameCount; ++i)
	{
		m_pColorBuffer[i].Reset();
	}
	m_pCmdList.Reset();
	for (auto i = 0u; i < FrameCount; ++i)
	{
		m_pCmdAllocator[i].Reset();
	}
	m_pSwapChain.Reset();
	m_pQueue.Reset();
	m_pDevice.Reset();
}

void App::OnTerm()
{
	for (auto i = 0; i < FrameCount; ++i)
	{
		if (m_pCB[i].Get() != nullptr)
		{
			m_pCB[i]->Unmap(0, nullptr);
			memset(&m_CBV[i], 0, sizeof(m_CBV[i]));
		}
		m_pCB[i].Reset();
	}
	m_pVB.Reset();
	m_pPSO.Reset();
}