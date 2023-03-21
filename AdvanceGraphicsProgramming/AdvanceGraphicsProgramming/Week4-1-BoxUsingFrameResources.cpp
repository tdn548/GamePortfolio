/** @file Week4-1-BoxUsingFrameResources.cpp
 *  @brief Draw a box using FrameResources.
 *
 *   With frame resources, we modify our render loop so that we
 *   do not have to flush the command queue every frame; this improves CPU and GPU utilization.
 *   Because the CPU only needs to modify constant buffers in this demo, the frame
 *   resource class only contains constant buffers
 *   Then we create a render item and we divide up our constant data based on update frequency.
 *
 *   Controls:
 *   Hold down '1' key to view scene in wireframe mode. 
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @author Hooman Salamat
 */

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//step3: Our application class instantiates a vector of three frame resources, 
const int gNumFrameResources = 3;

// Step10: Lightweight structure stores parameters to draw a shape.  This will vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify object data, we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	// Geometry associated with this render-item. Note that multiple
	// render-items can share the same geometry.
	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0; //Number of indices read from the index buffer for each instance.
	UINT StartIndexLocation = 0; //The location of the first index read by the GPU from the index buffer.
	int BaseVertexLocation = 0; //A value added to each index before reading a vertex from the vertex buffer.
};

enum class RenderLayer : int
{
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout(); // this
	void BuildTreeSpritesGeometry(); // this
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildWavesGeometry();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	//step4: keep member variables to track the current frame resource :
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	//step11: Our application will maintain lists of render items based on how they need to be
	//drawn; that is, render items that need different PSOs will be kept in different lists.

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	std::unique_ptr<Waves> mWaves;

	//step12: this mMainPassCB stores constant data that is fixed over a given
	//rendering pass such as the eye position, the view and projection matrices, and information
	//about the screen(render target) dimensions; it also includes game timing information,
	//which is useful data to have access to in shader programs.

	PassConstants mMainPassCB;

	UINT mPassCbvOffset = 0;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.25f * XM_PI;
	float mPhi = 0.3f * XM_PI;
	float mRadius = 50.0f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShapesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
  // so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildWavesGeometry();
	BuildTreeSpritesGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

//step7: for CPU frame n, the algorithm
//1. Cycle through the circular frame resource array.
//2. Wait until the GPU has completed commands up to this fence point.
//3. Update resources in mCurrFrameResource (like cbuffers).

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateWaves(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	//! Determines whether a key is up or down at the time the function is called, and whether the key was pressed after a previous call to GetAsyncKeyState.
	//! If the function succeeds, the return value specifies whether the key was pressed since the last call to GetAsyncKeyState, 
	//! and whether the key is currently up or down. If the most significant bit is set, the key is down, and if the least significant bit is set, the key was pressed after the previous call to GetAsyncKeyState.
	//! if (GetAsyncKeyState('1') & 0x8000) 

	short key = GetAsyncKeyState('1');
	//! you can use the virtual key code (0x31) for '1' as well
	//! https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	//! short key = GetAsyncKeyState(0x31); 

	if (key & 0x8000)  //if one is pressed, 0x8000 = 32767 , key = -32767 = FFFFFFFFFFFF8001
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water0"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

//step8: Update resources (cbuffers) in mCurrFrameResource
void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

//CBVs will be set at different frequencies—the per pass CBV only needs to be set once per
//rendering pass while the per object CBV needs to be set per render item
void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.95f, 0.95f, 0.95f, 1.0f };

	//directional light
	mMainPassCB.Lights[0].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.95f, 0.95f, 0.95f };


	//point light at columns

	mMainPassCB.Lights[1].Position = { 11.0f, 7.5f, -11.0f }; //front left
	mMainPassCB.Lights[1].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 

	mMainPassCB.Lights[2].Position = { 11.0f, 7.5f, 11.0f }; // back left
	mMainPassCB.Lights[2].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 

	mMainPassCB.Lights[3].Position = { -11.0f, 7.5f, -11.0f }; //front left
	mMainPassCB.Lights[3].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 

	mMainPassCB.Lights[4].Position = { -11.0f, 7.5f, 11.0f }; // back left
	mMainPassCB.Lights[4].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 


	// spot light at door
	mMainPassCB.Lights[5].Position = { 0.0f, 5.0f, -11.0f };
	mMainPassCB.Lights[5].Direction = { 0.0f, -1.0f,-1.0f };
	mMainPassCB.Lights[5].Strength = { 1.0f, 0.95f, 0.35f };
	mMainPassCB.Lights[5].SpotPower = 0.95f;



	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void ShapesApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"../../Textures/water2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->Filename = L"../../Textures/tile.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tileTex->Filename.c_str(),
		tileTex->Resource, tileTex->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[tileTex->Name] = std::move(tileTex);
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,  // number of descriptors
		0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0); // register b0
	slotRootParameter[2].InitAsConstantBufferView(1); // register b1
	slotRootParameter[3].InitAsConstantBufferView(2); // register b2

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void ShapesApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 5;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto tileTex = mTextures["tileTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = waterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = waterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = tileTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);
}





void ShapesApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void ShapesApp::BuildWavesGeometry()
{
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void ShapesApp::BuildTreeSpritesGeometry()
{
	//step5
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	float x, y, z;

	static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		if (i < 8)
		{
			x = -22;
			z = -22.5 + (i * 6);
			y = 6;
		}
		else
		{
			int index = i - 8;
			x = 22;
			z = -22.5 + (index * 6);
			y = 6;
		}

		// Move tree slightly above land height.
		//y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(8.0f, 8.0f);
	}

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}


//step16
void ShapesApp::BuildShapeGeometry()
{
	//GeometryGenerator is a utility class for generating simple geometric shapes like grids, sphere, cylinders, and boxes
	GeometryGenerator geoGen;
	//The MeshData structure is a simple structure nested inside GeometryGenerator that stores a vertex and index list
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 10, 20);
	// Creating a cone shape using the Cylinder geometry generator with top face radius value = 0
	GeometryGenerator::MeshData cone = geoGen.CreateCylinder(1.0, 0.0, 1.0, 20, 20);
	// Creating a cut cone shape using the Cylinder geometry generator with top face radius value half of the buttom face radius
	GeometryGenerator::MeshData cylinder2 = geoGen.CreateCylinder(1.0, 1.0, 1.0, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.0, 1.0, 1.0, 20, 20);
	GeometryGenerator::MeshData geosphere = geoGen.CreateGeosphere(1.0, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(30.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData floor = geoGen.CreateGrid(22.0f, 22.0f, 60, 40);
	GeometryGenerator::MeshData rectBattlements = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT sphereVertexOffset = (UINT)box.Vertices.size();
	UINT coneVertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size();
	UINT cylinder2VertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() + (UINT)cone.Vertices.size();
	UINT cylinderVertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() + 
		(UINT)cone.Vertices.size() + (UINT)cylinder2.Vertices.size();
	UINT geosphereVertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() +
		(UINT)cone.Vertices.size() + (UINT)cylinder2.Vertices.size() + (UINT)cylinder.Vertices.size();
	UINT gridVertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() +
		(UINT)cone.Vertices.size() + (UINT)cylinder2.Vertices.size() + (UINT)cylinder.Vertices.size() + (UINT)geosphere.Vertices.size();
	UINT floorVertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() + (UINT)cone.Vertices.size() + 
		(UINT)cylinder2.Vertices.size() + (UINT)cylinder.Vertices.size() + (UINT)geosphere.Vertices.size() + (UINT)grid.Vertices.size();
	UINT box2VertexOffset = (UINT)box.Vertices.size() + (UINT)sphere.Vertices.size() + (UINT)cone.Vertices.size() +
		(UINT)cylinder2.Vertices.size() + (UINT)cylinder.Vertices.size() + (UINT)geosphere.Vertices.size() + (UINT)grid.Vertices.size()
		+ (UINT)floor.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT sphereIndexOffset = (UINT)box.Indices32.size();
	UINT coneIndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size();
	UINT cylinder2IndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() + (UINT)cone.Indices32.size();
	UINT cylinderIndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() +
		(UINT)cone.Indices32.size() + (UINT)cylinder2.Indices32.size();
	UINT geosphereIndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() +
		(UINT)cone.Indices32.size() + (UINT)cylinder2.Indices32.size() + (UINT)cylinder.Indices32.size();
	UINT gridIndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() +
		(UINT)cone.Indices32.size() + (UINT)cylinder2.Indices32.size() + (UINT)cylinder.Indices32.size() + (UINT)geosphere.Indices32.size();
	UINT floorIndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() + (UINT)cone.Indices32.size() +
		(UINT)cylinder2.Indices32.size() + (UINT)cylinder.Indices32.size() + (UINT)geosphere.Indices32.size() + (UINT)grid.Indices32.size();
	UINT box2IndexOffset = (UINT)box.Indices32.size() + (UINT)sphere.Indices32.size() + (UINT)cone.Indices32.size() +
		(UINT)cylinder2.Indices32.size() + (UINT)cylinder.Indices32.size() + (UINT)geosphere.Indices32.size() + (UINT)grid.Indices32.size()
		+ (UINT)floor.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry coneSubmesh;
	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
	coneSubmesh.StartIndexLocation = coneIndexOffset;
	coneSubmesh.BaseVertexLocation = coneVertexOffset;

	SubmeshGeometry cylinder2Submesh;
	cylinder2Submesh.IndexCount = (UINT)cylinder2.Indices32.size();
	cylinder2Submesh.StartIndexLocation = cylinder2IndexOffset;
	cylinder2Submesh.BaseVertexLocation = cylinder2VertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry geosphereSubmesh;
	geosphereSubmesh.IndexCount = (UINT)geosphere.Indices32.size();
	geosphereSubmesh.StartIndexLocation = geosphereIndexOffset;
	geosphereSubmesh.BaseVertexLocation = geosphereVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = (UINT)floor.Indices32.size();
	floorSubmesh.StartIndexLocation = floorIndexOffset;
	floorSubmesh.BaseVertexLocation = floorVertexOffset;


	SubmeshGeometry box2Submesh;
	box2Submesh.IndexCount = (UINT)rectBattlements.Indices32.size();
	box2Submesh.StartIndexLocation = box2IndexOffset;
	box2Submesh.BaseVertexLocation = box2VertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount = box.Vertices.size() + sphere.Vertices.size() + cone.Vertices.size() 
		+ cylinder2.Vertices.size() + cylinder.Vertices.size() + grid.Vertices.size() + geosphere.Vertices.size() + floor.Vertices.size()
		+ rectBattlements.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder2.Vertices[i].Position;
		vertices[k].Normal = cylinder2.Vertices[i].Normal;
		vertices[k].TexC = cylinder2.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	for (size_t i = 0; i < geosphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = geosphere.Vertices[i].Position;
		vertices[k].Normal = geosphere.Vertices[i].Normal;
		vertices[k].TexC = geosphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < floor.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = floor.Vertices[i].Position;
		vertices[k].Normal = floor.Vertices[i].Normal;
		vertices[k].TexC = floor.Vertices[i].TexC;
	}

	for (size_t i = 0; i < rectBattlements.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = rectBattlements.Vertices[i].Position;
		vertices[k].Normal = rectBattlements.Vertices[i].Normal;
		vertices[k].TexC = rectBattlements.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder2.GetIndices16()), std::end(cylinder2.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(geosphere.GetIndices16()), std::end(geosphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(floor.GetIndices16()), std::end(floor.GetIndices16()));
	indices.insert(indices.end(), std::begin(rectBattlements.GetIndices16()), std::end(rectBattlements.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["battlement"] = box2Submesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["cylinder2"] = cylinder2Submesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["geosphere"] = geosphereSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["floor"] = floorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));



	//
	// PSO for tree sprites
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};
	//step1
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
}

//step6: build three frame resources
//FrameResource constructor:     FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
	}
}

void ShapesApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto water0 = std::make_unique<Material>();
	water0->Name = "water0";
	water0->MatCBIndex = 1;
	water0->DiffuseSrvHeapIndex = 1;
	water0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	water0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	water0->Roughness = 0.0f;

	auto grass0 = std::make_unique<Material>();
	grass0->Name = "grass0";
	grass0->MatCBIndex = 2;
	grass0->DiffuseSrvHeapIndex = 2;
	grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass0->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass0->Roughness = 0.125f;

	auto treeSprites0 = std::make_unique<Material>();
	treeSprites0->Name = "treeSprites0";
	treeSprites0->MatCBIndex = 3;
	treeSprites0->DiffuseSrvHeapIndex = 3;
	treeSprites0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites0->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites0->Roughness = 0.125f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["water0"] = std::move(water0);
	mMaterials["grass0"] = std::move(grass0);
	mMaterials["treeSprites0"] = std::move(treeSprites0);
}

void ShapesApp::BuildRenderItems()
{
	auto leftWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftWallRitem->World, XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(-11.0f, 4.0f, 0.0f));
	leftWallRitem->ObjCBIndex = 0;
	leftWallRitem->Mat = mMaterials["bricks0"].get();
	leftWallRitem->Geo = mGeometries["shapeGeo"].get();
	leftWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftWallRitem->IndexCount = leftWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	leftWallRitem->StartIndexLocation = leftWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	leftWallRitem->BaseVertexLocation = leftWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(leftWallRitem));

	auto rightWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&rightWallRitem->World, XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(11.0f, 4.0f, 0.0f));
	rightWallRitem->ObjCBIndex = 1;
	rightWallRitem->Mat = mMaterials["bricks0"].get();
	rightWallRitem->Geo = mGeometries["shapeGeo"].get();
	rightWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightWallRitem->IndexCount = rightWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	rightWallRitem->StartIndexLocation = rightWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	rightWallRitem->BaseVertexLocation = rightWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(rightWallRitem));

	auto backWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backWallRitem->World, XMMatrixScaling(20.0f, 4.0f, 1.0f) * XMMatrixTranslation(0.0f, 4.0f, 11.0f));
	backWallRitem->ObjCBIndex = 2;
	backWallRitem->Mat = mMaterials["bricks0"].get();
	backWallRitem->Geo = mGeometries["shapeGeo"].get();
	backWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backWallRitem->IndexCount = backWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	backWallRitem->StartIndexLocation = backWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	backWallRitem->BaseVertexLocation = backWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backWallRitem));

	auto frontLeftWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontLeftWallRitem->World, XMMatrixScaling(7.0f, 4.0f, 1.0f) * XMMatrixTranslation(-6.5f, 4.0f, -11.0f));
	frontLeftWallRitem->ObjCBIndex = 3;
	frontLeftWallRitem->Mat = mMaterials["bricks0"].get();
	frontLeftWallRitem->Geo = mGeometries["shapeGeo"].get();
	frontLeftWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontLeftWallRitem->IndexCount = frontLeftWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	frontLeftWallRitem->StartIndexLocation = frontLeftWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	frontLeftWallRitem->BaseVertexLocation = frontLeftWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontLeftWallRitem));

	auto frontRightWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontRightWallRitem->World, XMMatrixScaling(7.0f, 4.0f, 1.0f) * XMMatrixTranslation(6.5f, 4.0f, -11.0f));
	frontRightWallRitem->ObjCBIndex = 4;
	frontRightWallRitem->Mat = mMaterials["bricks0"].get();
	frontRightWallRitem->Geo = mGeometries["shapeGeo"].get();
	frontRightWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontRightWallRitem->IndexCount = frontRightWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	frontRightWallRitem->StartIndexLocation = frontRightWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	frontRightWallRitem->BaseVertexLocation = frontRightWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontRightWallRitem));

	auto upperDoorWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&upperDoorWallRitem->World, XMMatrixScaling(6.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 5.5f, -11.0f));
	upperDoorWallRitem->ObjCBIndex = 5;
	upperDoorWallRitem->Mat = mMaterials["bricks0"].get();
	upperDoorWallRitem->Geo = mGeometries["shapeGeo"].get();
	upperDoorWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	upperDoorWallRitem->IndexCount = upperDoorWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	upperDoorWallRitem->StartIndexLocation = upperDoorWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	upperDoorWallRitem->BaseVertexLocation = upperDoorWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(upperDoorWallRitem));

	auto roofRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&roofRitem->World, XMMatrixScaling(22.5f, 0.5f, 22.5f) * XMMatrixTranslation(0.0f, 5.75f, 0.0f));
	roofRitem->ObjCBIndex = 6;
	roofRitem->Mat = mMaterials["bricks0"].get();
	roofRitem->Geo = mGeometries["shapeGeo"].get();
	roofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	roofRitem->IndexCount = roofRitem->Geo->DrawArgs["box"].IndexCount;  //36
	roofRitem->StartIndexLocation = roofRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	roofRitem->BaseVertexLocation = roofRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(roofRitem));



	auto frontLeftColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontLeftColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(-11.0f, 4.5f, -11.0f));
	frontLeftColumnRitem->ObjCBIndex = 7;
	frontLeftColumnRitem->Mat = mMaterials["bricks0"].get();
	frontLeftColumnRitem->Geo = mGeometries["shapeGeo"].get();
	frontLeftColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontLeftColumnRitem->IndexCount = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	frontLeftColumnRitem->StartIndexLocation = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	frontLeftColumnRitem->BaseVertexLocation = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontLeftColumnRitem));

	auto frontrightColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontrightColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(11.0f, 4.5f, -11.0f));
	frontrightColumnRitem->ObjCBIndex = 8;
	frontrightColumnRitem->Mat = mMaterials["bricks0"].get();
	frontrightColumnRitem->Geo = mGeometries["shapeGeo"].get();
	frontrightColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontrightColumnRitem->IndexCount = frontrightColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	frontrightColumnRitem->StartIndexLocation = frontrightColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	frontrightColumnRitem->BaseVertexLocation = frontrightColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontrightColumnRitem));

	auto backLeftColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backLeftColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(-11.0f, 4.5f, 11.0f));
	backLeftColumnRitem->ObjCBIndex = 9;
	backLeftColumnRitem->Mat = mMaterials["bricks0"].get();
	backLeftColumnRitem->Geo = mGeometries["shapeGeo"].get();
	backLeftColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backLeftColumnRitem->IndexCount = backLeftColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	backLeftColumnRitem->StartIndexLocation = backLeftColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	backLeftColumnRitem->BaseVertexLocation = backLeftColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backLeftColumnRitem));

	auto backrightColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backrightColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(11.0f, 4.5f, 11.0f));
	backrightColumnRitem->ObjCBIndex = 10;
	backrightColumnRitem->Mat = mMaterials["bricks0"].get();
	backrightColumnRitem->Geo = mGeometries["shapeGeo"].get();
	backrightColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backrightColumnRitem->IndexCount = backrightColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	backrightColumnRitem->StartIndexLocation = backrightColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	backrightColumnRitem->BaseVertexLocation = backrightColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backrightColumnRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->ObjCBIndex = 11;
	gridRitem->Mat = mMaterials["bricks0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));


	auto floorRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&floorRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f)* XMMatrixTranslation(0.0f, 2.1f, 0.0f));
	floorRitem->ObjCBIndex = 12;
	floorRitem->Mat = mMaterials["bricks0"].get();
	floorRitem->Geo = mGeometries["shapeGeo"].get();
	floorRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mAllRitems.push_back(std::move(floorRitem));

	//Cone Column Battlement
	auto coneFrontLeft = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneFrontLeft->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(-11.0f, 8.0f, -11.0f));
	coneFrontLeft->ObjCBIndex = 13;
	coneFrontLeft->Mat = mMaterials["bricks0"].get();
	coneFrontLeft->Geo = mGeometries["shapeGeo"].get();
	coneFrontLeft->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneFrontLeft->IndexCount = coneFrontLeft->Geo->DrawArgs["cone"].IndexCount;  //36
	coneFrontLeft->StartIndexLocation = coneFrontLeft->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneFrontLeft->BaseVertexLocation = coneFrontLeft->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneFrontLeft));

	auto coneFrontRight= std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneFrontRight->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(11.0f, 8.0f, -11.0f));
	coneFrontRight->ObjCBIndex = 14;
	coneFrontRight->Mat = mMaterials["bricks0"].get();
	coneFrontRight->Geo = mGeometries["shapeGeo"].get();
	coneFrontRight->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneFrontRight->IndexCount = coneFrontRight->Geo->DrawArgs["cone"].IndexCount;  //36
	coneFrontRight->StartIndexLocation = coneFrontRight->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneFrontRight->BaseVertexLocation = coneFrontRight->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneFrontRight));


	auto coneBackLeft = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneBackLeft->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(-11.0f, 8.0f, 11.0f));
	coneBackLeft->ObjCBIndex = 15;
	coneBackLeft->Mat = mMaterials["bricks0"].get();
	coneBackLeft->Geo = mGeometries["shapeGeo"].get();
	coneBackLeft->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneBackLeft->IndexCount = coneBackLeft->Geo->DrawArgs["cone"].IndexCount;  //36
	coneBackLeft->StartIndexLocation = coneBackLeft->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneBackLeft->BaseVertexLocation = coneBackLeft->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneBackLeft));


	auto coneBackRight = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneBackRight->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(11.0f, 8.0f, 11.0f));
	coneBackRight->ObjCBIndex = 16;
	coneBackRight->Mat = mMaterials["bricks0"].get();
	coneBackRight->Geo = mGeometries["shapeGeo"].get();
	coneBackRight->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneBackRight->IndexCount = coneBackRight->Geo->DrawArgs["cone"].IndexCount;  //36
	coneBackRight->StartIndexLocation = coneBackRight->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneBackRight->BaseVertexLocation = coneBackRight->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneBackRight));

	 /*door*/ // * XMMatrixRotationX(-45.0f)
	auto door = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&door->World, XMMatrixScaling(6.0f, 3.0f, 0.5f)* XMMatrixTranslation(0.0f, 13.0f, -3.5f) * XMMatrixRotationX(-45.0f));
	door->ObjCBIndex = 17;
	door->Mat = mMaterials["bricks0"].get();
	door->Geo = mGeometries["shapeGeo"].get();
	door->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	door->IndexCount = door->Geo->DrawArgs["battlement"].IndexCount;  //36
	door->StartIndexLocation = door->Geo->DrawArgs["battlement"].StartIndexLocation; //0
	door->BaseVertexLocation = door->Geo->DrawArgs["battlement"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(door));

	// Cone front & back battlements
	float j = 0.0f;

	for (int i = 0; i < 12; i++)
	{
		float randNum1 = MathHelper::RandF(0.0f, 4.0f);
		float randNum2 = MathHelper::RandF(0.0f, 4.0f);

		//front
		auto coneFrontBattlement = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&coneFrontBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-8.9f + i + j, 6.5f, -11.0f));
		coneFrontBattlement->ObjCBIndex = 18 +i;

		if (randNum1 >= 0.0f && randNum1 < 3.0f)
		{
			coneFrontBattlement->Mat = mMaterials["bricks0"].get();
		}
		else
		{
			coneFrontBattlement->Mat = mMaterials["grass0"].get();
		}
		coneFrontBattlement->Geo = mGeometries["shapeGeo"].get();
		coneFrontBattlement->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		coneFrontBattlement->IndexCount = coneFrontBattlement->Geo->DrawArgs["cone"].IndexCount;  //36
		coneFrontBattlement->StartIndexLocation = coneFrontBattlement->Geo->DrawArgs["cone"].StartIndexLocation; //0
		coneFrontBattlement->BaseVertexLocation = coneFrontBattlement->Geo->DrawArgs["cone"].BaseVertexLocation; //0
		mAllRitems.push_back(std::move(coneFrontBattlement));


		////back
		auto coneBackBattlement = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&coneBackBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-8.9f + i + j, 6.5f, 11.0f));
		coneBackBattlement->ObjCBIndex = 30 + i;
		if (randNum2 >= 0.0f && randNum2 < 3.0f)
		{
			coneBackBattlement->Mat = mMaterials["bricks0"].get();
		}
		else
		{
			coneBackBattlement->Mat = mMaterials["grass0"].get();
		}
		coneBackBattlement->Geo = mGeometries["shapeGeo"].get();
		coneBackBattlement->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		coneBackBattlement->IndexCount = coneBackBattlement->Geo->DrawArgs["cone"].IndexCount;  //36
		coneBackBattlement->StartIndexLocation = coneBackBattlement->Geo->DrawArgs["cone"].StartIndexLocation; //0
		coneBackBattlement->BaseVertexLocation = coneBackBattlement->Geo->DrawArgs["cone"].BaseVertexLocation; //0
		mAllRitems.push_back(std::move(coneBackBattlement));
		j += 0.65f;
	/*	a = a + 1.6f;*/

	}

	float b = 0.0f;
	for (int a = 0; a < 9; a++)
	{
		float randNum1 = MathHelper::RandF(0.0f, 4.0f);
		float randNum2 = MathHelper::RandF(0.0f, 4.0f);

		// left
		auto cubeLeftBattlement = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&cubeLeftBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.5f) * XMMatrixTranslation(-10.5f , 6.5f, 8.9f - a - b));
		cubeLeftBattlement->ObjCBIndex = 42 + a;
		if (randNum1 >= 0.0f && randNum1 < 3.0f)
		{
			cubeLeftBattlement->Mat = mMaterials["bricks0"].get();
		}
		else
		{
			cubeLeftBattlement->Mat = mMaterials["grass0"].get();
		}
		cubeLeftBattlement->Geo = mGeometries["shapeGeo"].get();
		cubeLeftBattlement->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		cubeLeftBattlement->IndexCount = cubeLeftBattlement->Geo->DrawArgs["battlement"].IndexCount;  //36
		cubeLeftBattlement->StartIndexLocation = cubeLeftBattlement->Geo->DrawArgs["battlement"].StartIndexLocation; //0
		cubeLeftBattlement->BaseVertexLocation = cubeLeftBattlement->Geo->DrawArgs["battlement"].BaseVertexLocation; //0
		mAllRitems.push_back(std::move(cubeLeftBattlement));

		//Right
		auto cubeRightBattlement = std::make_unique<RenderItem>();
		XMStoreFloat4x4(&cubeRightBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.5f) * XMMatrixTranslation(10.5f, 6.5f, 8.9f - a - b));
		cubeRightBattlement->ObjCBIndex = 51 + a;
		if (randNum2 >= 0.0f && randNum2 < 3.0f)
		{
			cubeRightBattlement->Mat = mMaterials["bricks0"].get();
		}
		else
		{
			cubeRightBattlement->Mat = mMaterials["grass0"].get();
		}
		cubeRightBattlement->Geo = mGeometries["shapeGeo"].get();
		cubeRightBattlement->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		cubeRightBattlement->IndexCount = cubeRightBattlement->Geo->DrawArgs["battlement"].IndexCount;  //36
		cubeRightBattlement->StartIndexLocation = cubeRightBattlement->Geo->DrawArgs["battlement"].StartIndexLocation; //0
		cubeRightBattlement->BaseVertexLocation = cubeRightBattlement->Geo->DrawArgs["battlement"].BaseVertexLocation; //0
		mAllRitems.push_back(std::move(cubeRightBattlement));

		b +=1.25f;


	}

	// Door chains
	// right chain
	auto rightchain = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&rightchain->World, XMMatrixScaling(0.15f, 2.5f, 0.15f) * XMMatrixTranslation(2.5f,-8.0f,-10.5f) * XMMatrixRotationX(45.0f));
	rightchain->ObjCBIndex = 60;
	rightchain->Mat = mMaterials["grass0"].get();
	rightchain->Geo = mGeometries["shapeGeo"].get();
	rightchain->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightchain->IndexCount = rightchain->Geo->DrawArgs["cylinder2"].IndexCount;  //36
	rightchain->StartIndexLocation = rightchain->Geo->DrawArgs["cylinder2"].StartIndexLocation; //0
	rightchain->BaseVertexLocation = rightchain->Geo->DrawArgs["cylinder2"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(rightchain));

	// left chain
	auto leftchain = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftchain->World, XMMatrixScaling(0.15f, 2.5f, 0.15f) * XMMatrixTranslation(-2.5f, -8.0f, -10.5f) * XMMatrixRotationX(45.0f));
	leftchain->ObjCBIndex = 61;
	leftchain->Mat = mMaterials["grass0"].get();
	leftchain->Geo = mGeometries["shapeGeo"].get();
	leftchain->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftchain->IndexCount = leftchain->Geo->DrawArgs["cylinder2"].IndexCount;  //36
	leftchain->StartIndexLocation = leftchain->Geo->DrawArgs["cylinder2"].StartIndexLocation; //0
	leftchain->BaseVertexLocation = leftchain->Geo->DrawArgs["cylinder2"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(leftchain));


	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 62;
	wavesRitem->Mat = mMaterials["water0"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	// we use mVavesRitem in updatewaves() to set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem = wavesRitem.get();
	mAllRitems.push_back(std::move(wavesRitem));


	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&treeSpritesRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	treeSpritesRitem->ObjCBIndex = 63;
	treeSpritesRitem->Mat = mMaterials["treeSprites0"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	//step2
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());
	mAllRitems.push_back(std::move(treeSpritesRitem));


	//Add grass ground
	auto foundationitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&foundationitem->World, XMMatrixScaling(50.0f, 1.0f, 50.0f)* XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	foundationitem->ObjCBIndex = 64;
	foundationitem->Mat = mMaterials["grass0"].get();
	foundationitem->Geo = mGeometries["shapeGeo"].get();
	foundationitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	foundationitem->IndexCount = foundationitem->Geo->DrawArgs["box"].IndexCount;  //36
	foundationitem->StartIndexLocation = foundationitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	foundationitem->BaseVertexLocation = foundationitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(foundationitem));

//add brick foundation
	auto foundationitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&foundationitem2->World, XMMatrixScaling(40.0f, 1.0f, 40.0f)* XMMatrixTranslation(0.0f, 1.5f, 0.0f));
	foundationitem2->ObjCBIndex = 65;
	foundationitem2->Mat = mMaterials["bricks0"].get();
	foundationitem2->Geo = mGeometries["shapeGeo"].get();
	foundationitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	foundationitem2->IndexCount = foundationitem2->Geo->DrawArgs["box"].IndexCount;  //36
	foundationitem2->StartIndexLocation = foundationitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	foundationitem2->BaseVertexLocation = foundationitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(foundationitem2));

	//// All the render items are opaque.
	////Our application will maintain lists of render items based on how they need to be
	////drawn; that is, render items that need different PSOs will be kept in different lists.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
