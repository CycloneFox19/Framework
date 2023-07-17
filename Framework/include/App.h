#pragma once
#include <Windows.h>
#include <cstdint>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )

template<typename T> 
using ComPtr = Microsoft::WRL::ComPtr<T>;
struct alignas(256) Transform
{
	DirectX::XMMATRIX World;
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Proj;
};

template<typename T>
struct ConstantBufferView
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;
	D3D12_CPU_DESCRIPTOR_HANDLE HandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE HandleGPU;
	T* pBuffer;
};

class App
{

public:
	App(uint32_t width, uint32_t height);
	~App();
	void Run();
private:
	static const uint32_t FrameCount = 2;
	HINSTANCE m_hInst;
	HWND m_hWnd;
	uint32_t m_Width;
	uint32_t m_Height;
	ComPtr<ID3D12Device> m_pDevice;
	ComPtr<ID3D12CommandQueue> m_pQueue;
	ComPtr<IDXGISwapChain3> m_pSwapChain;
	ComPtr<ID3D12Resource> m_pColorBuffer[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_pCmdAllocator[FrameCount];
	ComPtr<ID3D12GraphicsCommandList> m_pCmdList;
	ComPtr<ID3D12DescriptorHeap> m_pHeapRTV;
	ComPtr<ID3D12Fence> m_pFence;
	ComPtr<ID3D12DescriptorHeap> m_pHeapCBV;
	ComPtr<ID3D12Resource> m_pVB;
	ComPtr<ID3D12Resource> m_pCB[FrameCount];
	ComPtr<ID3D12RootSignature> m_pRootSignature;
	ComPtr<ID3D12PipelineState> m_pPSO;
	HANDLE m_FenceEvent;
	uint64_t m_FenceCounter[FrameCount];
	uint32_t m_FrameIndex;
	D3D12_CPU_DESCRIPTOR_HANDLE m_HandleRTV[FrameCount];
	D3D12_VERTEX_BUFFER_VIEW m_VBV;
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	ConstantBufferView<Transform> m_CBV[FrameCount];
	float m_RotateAngle;
	bool InitApp();
	void TermApp();
	bool InitWnd();
	void TermWnd();
	void MainLoop();
	bool InitD3D();
	void TermD3D();
	void Render();
	void WaitGpu();
	void Present(uint32_t interval);
	bool OnInit();
	void OnTerm();
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM up, LPARAM lp);
};