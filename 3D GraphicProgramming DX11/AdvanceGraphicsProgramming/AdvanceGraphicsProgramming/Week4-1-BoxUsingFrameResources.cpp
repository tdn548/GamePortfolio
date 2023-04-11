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
#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "Waves.h"

#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//step3: Our application class instantiates a vector of three frame resources, 
const int gNumFrameResources = 3;

struct RenderCollisionRectangles
{
	float posX;
	float posZ;
	float sizeX;
	float sizeZ;
};

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

	// Collision
	std::list<RenderCollisionRectangles>rectangles;
	void AABBAABBCollision(const GameTimer& gt);
	float MinimumTranslationVector1D(const float centerA, const float radiusA, const float centerB, const float radiusB);
	float Sign(const float value);

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

	Camera mCamera;
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

	mCamera.SetPosition(0.0f, 4.5f, -53.0f);

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

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);;
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
	AABBAABBCollision(gt);
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
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	//GetAsyncKeyState returns a short (2 bytes)
	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		mCamera.RotateY(5.0f * dt);

	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		mCamera.RotateY(-5.0f * dt);

	mCamera.UpdateViewMatrix();
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
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
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

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
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
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

	mMainPassCB.Lights[1].Position = { 12.0f, 6.5f, 16.0f }; //front left
	mMainPassCB.Lights[1].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color
	mMainPassCB.Lights[1].FalloffStart = 3.0f;
	mMainPassCB.Lights[1].FalloffEnd = 5.0f;

	mMainPassCB.Lights[2].Position = { 12.0f, 6.5f, 44.0f }; // back left
	mMainPassCB.Lights[2].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 
	mMainPassCB.Lights[2].FalloffStart = 3.0f;
	mMainPassCB.Lights[2].FalloffEnd = 5.0f;

	mMainPassCB.Lights[3].Position = { -12.0f, 6.5f, 16.0f }; //front left
	mMainPassCB.Lights[3].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 
	mMainPassCB.Lights[3].FalloffStart = 3.0f;
	mMainPassCB.Lights[3].FalloffEnd = 5.0f;

	mMainPassCB.Lights[4].Position = { -12.0f, 6.5f, 44.0f }; // back left
	mMainPassCB.Lights[4].Strength = { 0.95f, 0.2f, 0.0f }; // yellow color 
	mMainPassCB.Lights[4].FalloffStart = 3.0f;
	mMainPassCB.Lights[4].FalloffEnd = 5.0f;


	// spot light at door
	mMainPassCB.Lights[5].Position = { 0.0f, 5.0f, 19.0f };
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

	auto bushTex = std::make_unique<Texture>();
	bushTex->Name = "bushTex";
	bushTex->Filename = L"../../Textures/Bush2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bushTex->Filename.c_str(),
		bushTex->Resource, bushTex->UploadHeap));

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
	mTextures[bushTex->Name] = std::move(bushTex);
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
	srvHeapDesc.NumDescriptors = 7;
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
	auto bushTex = mTextures["bushTex"]->Resource;
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
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = bushTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = bushTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(bushTex.Get(), &srvDesc, hDescriptor);

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

	static const int treeCount = 22;
	std::array<TreeSpriteVertex, 22> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		if (i < 8)
		{
			x = -22;
			z = 8.5f + (i * 6);
			y = 5.8;
		}
		else if(i >= 8 && i < 16)
		{
			int index = i - 8;
			x = 22;
			z = 8.5f + (index * 6);
			y = 5.8;
		}
		else
		{
			int index = i - 16;
			x = 17 - (index * 6);
			z = 52.0;
			y = 5.8;
		}

		// Move tree slightly above land height.
		//y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(8.0f, 8.0f);
	}

	std::array<std::uint16_t, 22> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
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

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.0f;

	auto grass0 = std::make_unique<Material>();
	grass0->Name = "grass0";
	grass0->MatCBIndex = 3;
	grass0->DiffuseSrvHeapIndex = 3;
	grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass0->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass0->Roughness = 0.125f;

	auto bush0 = std::make_unique<Material>();
	bush0->Name = "bush0";
	bush0->MatCBIndex = 4;
	bush0->DiffuseSrvHeapIndex = 4;
	bush0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bush0->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	bush0->Roughness = 0.125f;

	auto treeSprites0 = std::make_unique<Material>();
	treeSprites0->Name = "treeSprites0";
	treeSprites0->MatCBIndex = 5;
	treeSprites0->DiffuseSrvHeapIndex = 5;
	treeSprites0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites0->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites0->Roughness = 0.125f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["water0"] = std::move(water0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["grass0"] = std::move(grass0);
	mMaterials["bush0"] = std::move(bush0);
	mMaterials["treeSprites0"] = std::move(treeSprites0);
}

void ShapesApp::BuildRenderItems()
{
	auto leftWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftWallRitem->World, XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(-11.0f, 4.4f, 30.0f));
	leftWallRitem->ObjCBIndex = 0;
	leftWallRitem->Mat = mMaterials["bricks0"].get();
	leftWallRitem->Geo = mGeometries["shapeGeo"].get();
	leftWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftWallRitem->IndexCount = leftWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	leftWallRitem->StartIndexLocation = leftWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	leftWallRitem->BaseVertexLocation = leftWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(leftWallRitem));
	RenderCollisionRectangles leftWallRitemCol;
	leftWallRitemCol.posX = -11.0;
	leftWallRitemCol.posZ = 30.0;
	leftWallRitemCol.sizeX = 1.0;
	leftWallRitemCol.sizeZ = 20.0;
	rectangles.push_back(leftWallRitemCol);

	auto rightWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&rightWallRitem->World, XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(11.0f, 4.4f, 30.0f));
	rightWallRitem->ObjCBIndex = 1;
	rightWallRitem->Mat = mMaterials["bricks0"].get();
	rightWallRitem->Geo = mGeometries["shapeGeo"].get();
	rightWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightWallRitem->IndexCount = rightWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	rightWallRitem->StartIndexLocation = rightWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	rightWallRitem->BaseVertexLocation = rightWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(rightWallRitem));
	RenderCollisionRectangles rightWallRitemCol;
	rightWallRitemCol.posX = 11.0;
	rightWallRitemCol.posZ = 30.0;
	rightWallRitemCol.sizeX = 1.0;
	rightWallRitemCol.sizeZ = 20.0;
	rectangles.push_back(rightWallRitemCol);

	auto backWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backWallRitem->World, XMMatrixScaling(20.0f, 4.0f, 1.0f) * XMMatrixTranslation(0.0f, 4.4f, 41.0f));
	backWallRitem->ObjCBIndex = 2;
	backWallRitem->Mat = mMaterials["bricks0"].get();
	backWallRitem->Geo = mGeometries["shapeGeo"].get();
	backWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backWallRitem->IndexCount = backWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	backWallRitem->StartIndexLocation = backWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	backWallRitem->BaseVertexLocation = backWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backWallRitem));
	RenderCollisionRectangles backWallRitemCol;
	backWallRitemCol.posX = 0.0;
	backWallRitemCol.posZ = 41.0;
	backWallRitemCol.sizeX = 20.0;
	backWallRitemCol.sizeZ = 1.0;
	rectangles.push_back(backWallRitemCol);

	auto frontLeftWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontLeftWallRitem->World, XMMatrixScaling(7.0f, 4.0f, 1.0f) * XMMatrixTranslation(-6.5f, 4.4f, 19.05f));
	frontLeftWallRitem->ObjCBIndex = 3;
	frontLeftWallRitem->Mat = mMaterials["bricks0"].get();
	frontLeftWallRitem->Geo = mGeometries["shapeGeo"].get();
	frontLeftWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontLeftWallRitem->IndexCount = frontLeftWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	frontLeftWallRitem->StartIndexLocation = frontLeftWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	frontLeftWallRitem->BaseVertexLocation = frontLeftWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontLeftWallRitem));
	RenderCollisionRectangles frontLeftWallRitemCol;
	frontLeftWallRitemCol.posX = -6.5;
	frontLeftWallRitemCol.posZ = 19.0;
	frontLeftWallRitemCol.sizeX = 7.0;
	frontLeftWallRitemCol.sizeZ = 1.0;
	rectangles.push_back(frontLeftWallRitemCol);

	auto frontRightWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontRightWallRitem->World, XMMatrixScaling(7.0f, 4.0f, 1.0f) * XMMatrixTranslation(6.5f, 4.4f, 19.05f));
	frontRightWallRitem->ObjCBIndex = 4;
	frontRightWallRitem->Mat = mMaterials["bricks0"].get();
	frontRightWallRitem->Geo = mGeometries["shapeGeo"].get();
	frontRightWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontRightWallRitem->IndexCount = frontRightWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	frontRightWallRitem->StartIndexLocation = frontRightWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	frontRightWallRitem->BaseVertexLocation = frontRightWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontRightWallRitem));
	RenderCollisionRectangles frontRightWallRitemCol;
	frontRightWallRitemCol.posX = 6.5;
	frontRightWallRitemCol.posZ = 19.0;
	frontRightWallRitemCol.sizeX = 7.0;
	frontRightWallRitemCol.sizeZ = 1.0;
	rectangles.push_back(frontRightWallRitemCol);

	auto upperDoorWallRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&upperDoorWallRitem->World, XMMatrixScaling(6.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 5.9f, 19.0f));
	upperDoorWallRitem->ObjCBIndex = 5;
	upperDoorWallRitem->Mat = mMaterials["bricks0"].get();
	upperDoorWallRitem->Geo = mGeometries["shapeGeo"].get();
	upperDoorWallRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	upperDoorWallRitem->IndexCount = upperDoorWallRitem->Geo->DrawArgs["box"].IndexCount;  //36
	upperDoorWallRitem->StartIndexLocation = upperDoorWallRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	upperDoorWallRitem->BaseVertexLocation = upperDoorWallRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(upperDoorWallRitem));

	auto roofRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&roofRitem->World, XMMatrixScaling(22.5f, 0.5f, 22.5f) * XMMatrixTranslation(0.0f, 6.15f, 30.0f));
	roofRitem->ObjCBIndex = 6;
	roofRitem->Mat = mMaterials["bricks0"].get();
	roofRitem->Geo = mGeometries["shapeGeo"].get();
	roofRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	roofRitem->IndexCount = roofRitem->Geo->DrawArgs["box"].IndexCount;  //36
	roofRitem->StartIndexLocation = roofRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	roofRitem->BaseVertexLocation = roofRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(roofRitem));



	auto frontLeftColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontLeftColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(-11.0f, 4.9f, 19.0f));
	frontLeftColumnRitem->ObjCBIndex = 7;
	frontLeftColumnRitem->Mat = mMaterials["bricks0"].get();
	frontLeftColumnRitem->Geo = mGeometries["shapeGeo"].get();
	frontLeftColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontLeftColumnRitem->IndexCount = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	frontLeftColumnRitem->StartIndexLocation = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	frontLeftColumnRitem->BaseVertexLocation = frontLeftColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontLeftColumnRitem));
	RenderCollisionRectangles frontLeftColumnRitemCol;
	frontLeftColumnRitemCol.posX = -11.0;
	frontLeftColumnRitemCol.posZ = 19.0;
	frontLeftColumnRitemCol.sizeX = 3.0;
	frontLeftColumnRitemCol.sizeZ = 3.0;
	rectangles.push_back(frontLeftColumnRitemCol);

	auto frontrightColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&frontrightColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(11.0f, 4.9f, 19.0f));
	frontrightColumnRitem->ObjCBIndex = 8;
	frontrightColumnRitem->Mat = mMaterials["bricks0"].get();
	frontrightColumnRitem->Geo = mGeometries["shapeGeo"].get();
	frontrightColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	frontrightColumnRitem->IndexCount = frontrightColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	frontrightColumnRitem->StartIndexLocation = frontrightColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	frontrightColumnRitem->BaseVertexLocation = frontrightColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(frontrightColumnRitem));
	RenderCollisionRectangles frontrightColumnRitemCol;
	frontrightColumnRitemCol.posX = 11.0;
	frontrightColumnRitemCol.posZ = 19.0;
	frontrightColumnRitemCol.sizeX = 3.0;
	frontrightColumnRitemCol.sizeZ = 3.0;
	rectangles.push_back(frontrightColumnRitemCol);

	auto backLeftColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backLeftColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(-11.0f, 4.9f, 41.0f));
	backLeftColumnRitem->ObjCBIndex = 9;
	backLeftColumnRitem->Mat = mMaterials["bricks0"].get();
	backLeftColumnRitem->Geo = mGeometries["shapeGeo"].get();
	backLeftColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backLeftColumnRitem->IndexCount = backLeftColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	backLeftColumnRitem->StartIndexLocation = backLeftColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	backLeftColumnRitem->BaseVertexLocation = backLeftColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backLeftColumnRitem));
	RenderCollisionRectangles backLeftColumnRitemCol;
	backLeftColumnRitemCol.posX = -11.0;
	backLeftColumnRitemCol.posZ = 41.0;
	backLeftColumnRitemCol.sizeX = 3.0;
	backLeftColumnRitemCol.sizeZ = 3.0;
	rectangles.push_back(backLeftColumnRitemCol);

	auto backrightColumnRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&backrightColumnRitem->World, XMMatrixScaling(1.2f, 5.0f, 1.2f) * XMMatrixTranslation(11.0f, 4.9f, 41.0f));
	backrightColumnRitem->ObjCBIndex = 10;
	backrightColumnRitem->Mat = mMaterials["bricks0"].get();
	backrightColumnRitem->Geo = mGeometries["shapeGeo"].get();
	backrightColumnRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	backrightColumnRitem->IndexCount = backrightColumnRitem->Geo->DrawArgs["cylinder"].IndexCount;  //36
	backrightColumnRitem->StartIndexLocation = backrightColumnRitem->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	backrightColumnRitem->BaseVertexLocation = backrightColumnRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(backrightColumnRitem));
	RenderCollisionRectangles backrightColumnRitemCol;
	backrightColumnRitemCol.posX = 11.0;
	backrightColumnRitemCol.posZ = 41.0;
	backrightColumnRitemCol.sizeX = 3.0;
	backrightColumnRitemCol.sizeZ = 3.0;
	rectangles.push_back(backrightColumnRitemCol);

	//Cone Column Battlement
	auto coneFrontLeft = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneFrontLeft->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(-11.0f, 8.4f, 19.0f));
	coneFrontLeft->ObjCBIndex = 11;
	coneFrontLeft->Mat = mMaterials["bricks0"].get();
	coneFrontLeft->Geo = mGeometries["shapeGeo"].get();
	coneFrontLeft->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneFrontLeft->IndexCount = coneFrontLeft->Geo->DrawArgs["cone"].IndexCount;  //36
	coneFrontLeft->StartIndexLocation = coneFrontLeft->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneFrontLeft->BaseVertexLocation = coneFrontLeft->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneFrontLeft));

	auto coneFrontRight= std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneFrontRight->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(11.0f, 8.4f, 19.0f));
	coneFrontRight->ObjCBIndex = 12;
	coneFrontRight->Mat = mMaterials["bricks0"].get();
	coneFrontRight->Geo = mGeometries["shapeGeo"].get();
	coneFrontRight->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneFrontRight->IndexCount = coneFrontRight->Geo->DrawArgs["cone"].IndexCount;  //36
	coneFrontRight->StartIndexLocation = coneFrontRight->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneFrontRight->BaseVertexLocation = coneFrontRight->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneFrontRight));


	auto coneBackLeft = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneBackLeft->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(-11.0f, 8.4f, 41.0f));
	coneBackLeft->ObjCBIndex = 13;
	coneBackLeft->Mat = mMaterials["bricks0"].get();
	coneBackLeft->Geo = mGeometries["shapeGeo"].get();
	coneBackLeft->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneBackLeft->IndexCount = coneBackLeft->Geo->DrawArgs["cone"].IndexCount;  //36
	coneBackLeft->StartIndexLocation = coneBackLeft->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneBackLeft->BaseVertexLocation = coneBackLeft->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneBackLeft));


	auto coneBackRight = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneBackRight->World, XMMatrixScaling(1.5f, 2.0f, 1.5f)* XMMatrixTranslation(11.0f, 8.4f, 41.0f));
	coneBackRight->ObjCBIndex = 14;
	coneBackRight->Mat = mMaterials["bricks0"].get();
	coneBackRight->Geo = mGeometries["shapeGeo"].get();
	coneBackRight->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneBackRight->IndexCount = coneBackRight->Geo->DrawArgs["cone"].IndexCount;  //36
	coneBackRight->StartIndexLocation = coneBackRight->Geo->DrawArgs["cone"].StartIndexLocation; //0
	coneBackRight->BaseVertexLocation = coneBackRight->Geo->DrawArgs["cone"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(coneBackRight));

	 /*door*/ // * XMMatrixRotationX(-45.0f)
	auto door = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&door->World, XMMatrixScaling(6.0f, 3.0f, 0.5f) * XMMatrixRotationX(-45.0f) * XMMatrixTranslation(0.0f, 3.6f, 17.5f));
	door->ObjCBIndex = 15;
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
		XMStoreFloat4x4(&coneFrontBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-8.9f + i + j, 6.9f, 19.0f));
		coneFrontBattlement->ObjCBIndex = 16 +i;

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
		XMStoreFloat4x4(&coneBackBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-8.9f + i + j, 6.9f, 41.0f));
		coneBackBattlement->ObjCBIndex = 28 + i;
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
		XMStoreFloat4x4(&cubeLeftBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.5f) * XMMatrixTranslation(-10.5f , 6.9f, 38.9f - a - b));
		cubeLeftBattlement->ObjCBIndex = 40 + a;
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
		XMStoreFloat4x4(&cubeRightBattlement->World, XMMatrixScaling(1.0f, 1.0f, 1.5f) * XMMatrixTranslation(10.5f, 6.9f, 38.9f - a - b));
		cubeRightBattlement->ObjCBIndex = 49 + a;
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
	XMStoreFloat4x4(&rightchain->World, XMMatrixScaling(0.15f, 2.5f, 0.15f) * XMMatrixRotationX(45.0f) * XMMatrixTranslation(2.5f, 5.0f, 18.0f));
	rightchain->ObjCBIndex = 58;
	rightchain->Mat = mMaterials["grass0"].get();
	rightchain->Geo = mGeometries["shapeGeo"].get();
	rightchain->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rightchain->IndexCount = rightchain->Geo->DrawArgs["cylinder2"].IndexCount;  //36
	rightchain->StartIndexLocation = rightchain->Geo->DrawArgs["cylinder2"].StartIndexLocation; //0
	rightchain->BaseVertexLocation = rightchain->Geo->DrawArgs["cylinder2"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(rightchain));

	// left chain
	auto leftchain = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftchain->World, XMMatrixScaling(0.15f, 2.5f, 0.15f) * XMMatrixRotationX(45.0f) * XMMatrixTranslation(-2.5f, 5.0f, 18.0f));
	leftchain->ObjCBIndex = 59;
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
	wavesRitem->ObjCBIndex = 60;
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
	treeSpritesRitem->ObjCBIndex = 61;
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
	XMStoreFloat4x4(&foundationitem->World, XMMatrixScaling(60.0f, 1.5f, 115.0f)* XMMatrixTranslation(0.0f, 1.4f, 2.5f));
	foundationitem->ObjCBIndex = 62;
	foundationitem->Mat = mMaterials["grass0"].get();
	foundationitem->Geo = mGeometries["shapeGeo"].get();
	foundationitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	foundationitem->IndexCount = foundationitem->Geo->DrawArgs["box"].IndexCount;  //36
	foundationitem->StartIndexLocation = foundationitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	foundationitem->BaseVertexLocation = foundationitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(foundationitem));

//add brick foundation
	auto foundationitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&foundationitem2->World, XMMatrixScaling(40.0f, 0.3f, 40.0f)* XMMatrixTranslation(0.0f, 2.3f, 30.0f));
	foundationitem2->ObjCBIndex = 63;
	foundationitem2->Mat = mMaterials["bricks0"].get();
	foundationitem2->Geo = mGeometries["shapeGeo"].get();
	foundationitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	foundationitem2->IndexCount = foundationitem2->Geo->DrawArgs["box"].IndexCount;  //36
	foundationitem2->StartIndexLocation = foundationitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	foundationitem2->BaseVertexLocation = foundationitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(foundationitem2));


	///MAZE

	auto leftMazeRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&leftMazeRitem->World, XMMatrixScaling(1.0f, 4.0f, 50.0f)* XMMatrixTranslation(24.5f, 4.1f, -20.5f));
	leftMazeRitem->ObjCBIndex = 64;
	leftMazeRitem->Mat = mMaterials["bush0"].get();
	leftMazeRitem->Geo = mGeometries["shapeGeo"].get();
	leftMazeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	leftMazeRitem->IndexCount = leftMazeRitem->Geo->DrawArgs["box"].IndexCount;  //36
	leftMazeRitem->StartIndexLocation = leftMazeRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	leftMazeRitem->BaseVertexLocation = leftMazeRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	RenderCollisionRectangles leftMazeRitemCol;
	leftMazeRitemCol.posX = 24.5;
	leftMazeRitemCol.posZ = -20.5;
	leftMazeRitemCol.sizeX = 1.0;
	leftMazeRitemCol.sizeZ = 50.0;
	rectangles.push_back(leftMazeRitemCol);
	mAllRitems.push_back(std::move(leftMazeRitem));

	auto RighttMazeRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&RighttMazeRitem->World, XMMatrixScaling(1.0f, 4.0f, 50.0f)* XMMatrixTranslation(-24.5f, 4.1f, -20.5f));
	RighttMazeRitem->ObjCBIndex = 65;
	RighttMazeRitem->Mat = mMaterials["bush0"].get();
	RighttMazeRitem->Geo = mGeometries["shapeGeo"].get();
	RighttMazeRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RighttMazeRitem->IndexCount = RighttMazeRitem->Geo->DrawArgs["box"].IndexCount;  //36
	RighttMazeRitem->StartIndexLocation = RighttMazeRitem->Geo->DrawArgs["box"].StartIndexLocation; //0
	RighttMazeRitem->BaseVertexLocation = RighttMazeRitem->Geo->DrawArgs["box"].BaseVertexLocation; //0
	RenderCollisionRectangles RighttMazeRitemCol;
	RighttMazeRitemCol.posX = -24.5;
	RighttMazeRitemCol.posZ = -20.5;
	RighttMazeRitemCol.sizeX = 1.0;
	RighttMazeRitemCol.sizeZ = 50.0;
	rectangles.push_back(RighttMazeRitemCol);
	mAllRitems.push_back(std::move(RighttMazeRitem));

	auto FontMazeRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&FontMazeRitem1->World, XMMatrixScaling(28.0f, 4.0f, 1.0f)* XMMatrixTranslation(-16.0f, 4.1f, -45.1f));
	FontMazeRitem1->ObjCBIndex = 66;
	FontMazeRitem1->Mat = mMaterials["bush0"].get();
	FontMazeRitem1->Geo = mGeometries["shapeGeo"].get();
	FontMazeRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FontMazeRitem1->IndexCount = FontMazeRitem1->Geo->DrawArgs["box"].IndexCount;  //36
	FontMazeRitem1->StartIndexLocation = FontMazeRitem1->Geo->DrawArgs["box"].StartIndexLocation; //0
	FontMazeRitem1->BaseVertexLocation = FontMazeRitem1->Geo->DrawArgs["box"].BaseVertexLocation; //0
	RenderCollisionRectangles FontMazeRitem1Col;
	FontMazeRitem1Col.posX = -16.0;
	FontMazeRitem1Col.posZ = -45.1;
	FontMazeRitem1Col.sizeX = 28.0;
	FontMazeRitem1Col.sizeZ = 1.0;
	rectangles.push_back(FontMazeRitem1Col);
	mAllRitems.push_back(std::move(FontMazeRitem1));

	auto FontMazeRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&FontMazeRitem2->World, XMMatrixScaling(28.0f, 4.0f, 1.0f)* XMMatrixTranslation(16.0f, 4.1f, -45.1f));
	FontMazeRitem2->ObjCBIndex = 67;
	FontMazeRitem2->Mat = mMaterials["bush0"].get();
	FontMazeRitem2->Geo = mGeometries["shapeGeo"].get();
	FontMazeRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	FontMazeRitem2->IndexCount = FontMazeRitem2->Geo->DrawArgs["box"].IndexCount;  //36
	FontMazeRitem2->StartIndexLocation = FontMazeRitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	FontMazeRitem2->BaseVertexLocation = FontMazeRitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	RenderCollisionRectangles FontMazeRitem2Col;
	FontMazeRitem2Col.posX = 16.0;
	FontMazeRitem2Col.posZ = -45.1;
	FontMazeRitem2Col.sizeX = 28.0;
	FontMazeRitem2Col.sizeZ = 1.0;
	rectangles.push_back(FontMazeRitem2Col);
	mAllRitems.push_back(std::move(FontMazeRitem2));

	auto BackMazeRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&BackMazeRitem1->World, XMMatrixScaling(25.0f, 4.0f, 1.0f)* XMMatrixTranslation(-12.0f, 4.1f, 4.0f));
	BackMazeRitem1->ObjCBIndex = 68;
	BackMazeRitem1->Mat = mMaterials["bush0"].get();
	BackMazeRitem1->Geo = mGeometries["shapeGeo"].get();
	BackMazeRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	BackMazeRitem1->IndexCount = BackMazeRitem1->Geo->DrawArgs["box"].IndexCount;  //36
	BackMazeRitem1->StartIndexLocation = BackMazeRitem1->Geo->DrawArgs["box"].StartIndexLocation; //0
	BackMazeRitem1->BaseVertexLocation = BackMazeRitem1->Geo->DrawArgs["box"].BaseVertexLocation; //0
	RenderCollisionRectangles BackMazeRitem1Col;
	BackMazeRitem1Col.posX = -12.0;
	BackMazeRitem1Col.posZ = 4.0;
	BackMazeRitem1Col.sizeX = 25.0;
	BackMazeRitem1Col.sizeZ = 1.0;
	rectangles.push_back(BackMazeRitem1Col);
	mAllRitems.push_back(std::move(BackMazeRitem1));

	auto BackMazeRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&BackMazeRitem2->World, XMMatrixScaling(15.0f, 4.0f, 1.0f)* XMMatrixTranslation(17.0f, 4.1f, 4.0f));
	BackMazeRitem2->ObjCBIndex = 69;
	BackMazeRitem2->Mat = mMaterials["bush0"].get();
	BackMazeRitem2->Geo = mGeometries["shapeGeo"].get();
	BackMazeRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	BackMazeRitem2->IndexCount = BackMazeRitem2->Geo->DrawArgs["box"].IndexCount;  //36
	BackMazeRitem2->StartIndexLocation = BackMazeRitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	BackMazeRitem2->BaseVertexLocation = BackMazeRitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(BackMazeRitem2));
	RenderCollisionRectangles BackMazeRitem2Col;
	BackMazeRitem2Col.posX = 17.0;
	BackMazeRitem2Col.posZ = 4.0;
	BackMazeRitem2Col.sizeX = 15.0;
	BackMazeRitem2Col.sizeZ = 1.0;
	rectangles.push_back(BackMazeRitem2Col);


	auto roundabout = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&roundabout->World, XMMatrixScaling(4.0f, 4.0f, 4.0f)* XMMatrixTranslation(0.0f, 4.1f, -22.0f));
	roundabout->ObjCBIndex = 70;
	roundabout->Mat = mMaterials["bush0"].get();
	roundabout->Geo = mGeometries["shapeGeo"].get();
	roundabout->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	roundabout->IndexCount = roundabout->Geo->DrawArgs["cylinder"].IndexCount;  //36
	roundabout->StartIndexLocation = roundabout->Geo->DrawArgs["cylinder"].StartIndexLocation; //0
	roundabout->BaseVertexLocation = roundabout->Geo->DrawArgs["cylinder"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(roundabout));
	RenderCollisionRectangles roundaboutCol;
	roundaboutCol.posX = 0.0;
	roundaboutCol.posZ = -22.0;
	roundaboutCol.sizeX = 8.0;
	roundaboutCol.sizeZ = 8.0;
	rectangles.push_back(roundaboutCol);


	//two long horizon maze item 
	auto HorizonMazeitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem1->World, XMMatrixScaling(40.0f, 4.0f, 1.0f)* XMMatrixTranslation(0.0f, 4.1f, -2.0f));
	HorizonMazeitem1->ObjCBIndex = 71;
	HorizonMazeitem1->Mat = mMaterials["bush0"].get();
	HorizonMazeitem1->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem1->IndexCount = HorizonMazeitem1->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem1->StartIndexLocation = HorizonMazeitem1->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem1->BaseVertexLocation = HorizonMazeitem1->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem1));
	RenderCollisionRectangles HorizonMazeitem1Col;
	HorizonMazeitem1Col.posX = 0.0;
	HorizonMazeitem1Col.posZ = -2.0;
	HorizonMazeitem1Col.sizeX = 40.0;
	HorizonMazeitem1Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem1Col);

	auto HorizonMazeitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem2->World, XMMatrixScaling(40.0f, 4.0f, 1.0f)* XMMatrixTranslation(0.0f, 4.1f, -38.0f));
	HorizonMazeitem2->ObjCBIndex = 72;
	HorizonMazeitem2->Mat = mMaterials["bush0"].get();
	HorizonMazeitem2->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem2->IndexCount = HorizonMazeitem2->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem2->StartIndexLocation = HorizonMazeitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem2->BaseVertexLocation = HorizonMazeitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem2));
	RenderCollisionRectangles HorizonMazeitem2Col;
	HorizonMazeitem2Col.posX = 0.0;
	HorizonMazeitem2Col.posZ = -38.0;
	HorizonMazeitem2Col.sizeX = 40.0;
	HorizonMazeitem2Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem2Col);

	//five short horizon maze item 
	auto HorizonMazeitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem3->World, XMMatrixScaling(15.0f, 4.0f, 1.0f)* XMMatrixTranslation(10.0f, 4.1f, -8.0f));
	HorizonMazeitem3->ObjCBIndex = 73;
	HorizonMazeitem3->Mat = mMaterials["bush0"].get();
	HorizonMazeitem3->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem3->IndexCount = HorizonMazeitem3->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem3->StartIndexLocation = HorizonMazeitem3->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem3->BaseVertexLocation = HorizonMazeitem3->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem3));
	RenderCollisionRectangles HorizonMazeitem3Col;
	HorizonMazeitem3Col.posX = 10.0;
	HorizonMazeitem3Col.posZ = -8.0;
	HorizonMazeitem3Col.sizeX = 15.0;
	HorizonMazeitem3Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem3Col);

	auto HorizonMazeitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem4->World, XMMatrixScaling(15.0f, 4.0f, 1.0f)* XMMatrixTranslation(-10.0f, 4.1f, -8.1f));
	HorizonMazeitem4->ObjCBIndex = 74;
	HorizonMazeitem4->Mat = mMaterials["bush0"].get();
	HorizonMazeitem4->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem4->IndexCount = HorizonMazeitem4->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem4->StartIndexLocation = HorizonMazeitem4->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem4->BaseVertexLocation = HorizonMazeitem4->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem4));
	RenderCollisionRectangles HorizonMazeitem4Col;
	HorizonMazeitem4Col.posX = -10.0;
	HorizonMazeitem4Col.posZ = -8.1;
	HorizonMazeitem4Col.sizeX = 15.0;
	HorizonMazeitem4Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem4Col);


	auto HorizonMazeitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem5->World, XMMatrixScaling(30.0f, 4.0f, 1.0f)* XMMatrixTranslation(-3.0f, 4.1f, -32.0f));
	HorizonMazeitem5->ObjCBIndex = 75;
	HorizonMazeitem5->Mat = mMaterials["bush0"].get();
	HorizonMazeitem5->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem5->IndexCount = HorizonMazeitem5->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem5->StartIndexLocation = HorizonMazeitem5->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem5->BaseVertexLocation = HorizonMazeitem5->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem5));
	RenderCollisionRectangles HorizonMazeitem5Col;
	HorizonMazeitem5Col.posX = -3.0;
	HorizonMazeitem5Col.posZ = -32.0;
	HorizonMazeitem5Col.sizeX = 30.0;
	HorizonMazeitem5Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem5Col);

	///short center items
	auto HorizonMazeitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem6->World, XMMatrixScaling(6.0f, 4.0f, 1.0f)* XMMatrixTranslation(21.0f, 4.1f, -19.0f));
	HorizonMazeitem6->ObjCBIndex = 76;
	HorizonMazeitem6->Mat = mMaterials["bush0"].get();
	HorizonMazeitem6->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem6->IndexCount = HorizonMazeitem6->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem6->StartIndexLocation = HorizonMazeitem6->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem6->BaseVertexLocation = HorizonMazeitem6->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem6));
	RenderCollisionRectangles HorizonMazeitem6Col;
	HorizonMazeitem6Col.posX = 21.0;
	HorizonMazeitem6Col.posZ = -19.0;
	HorizonMazeitem6Col.sizeX = 6.0;
	HorizonMazeitem6Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem6Col);

	auto HorizonMazeitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem7->World, XMMatrixScaling(6.0f, 4.0f, 1.0f)* XMMatrixTranslation(-21.0f, 4.1f, -19.1f));
	HorizonMazeitem7->ObjCBIndex = 77;
	HorizonMazeitem7->Mat = mMaterials["bush0"].get();
	HorizonMazeitem7->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem7->IndexCount = HorizonMazeitem7->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem7->StartIndexLocation = HorizonMazeitem7->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem7->BaseVertexLocation = HorizonMazeitem7->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem7));
	RenderCollisionRectangles HorizonMazeitem7Col;
	HorizonMazeitem7Col.posX = -21.0;
	HorizonMazeitem7Col.posZ = -19.1;
	HorizonMazeitem7Col.sizeX = 6.0;
	HorizonMazeitem7Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem7Col);

	//five short vertical maze item 
	auto VerticalMazeitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&VerticalMazeitem1->World, XMMatrixScaling(1.0f,4.0f, 6.0f)* XMMatrixTranslation(-10.0f, 4.1f, 1.0f));
	VerticalMazeitem1->ObjCBIndex = 78;
	VerticalMazeitem1->Mat = mMaterials["bush0"].get();
	VerticalMazeitem1->Geo = mGeometries["shapeGeo"].get();
	VerticalMazeitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	VerticalMazeitem1->IndexCount = VerticalMazeitem1->Geo->DrawArgs["box"].IndexCount;  //36
	VerticalMazeitem1->StartIndexLocation = VerticalMazeitem1->Geo->DrawArgs["box"].StartIndexLocation; //0
	VerticalMazeitem1->BaseVertexLocation = VerticalMazeitem1->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(VerticalMazeitem1));
	RenderCollisionRectangles VerticalMazeitem1Col;
	VerticalMazeitem1Col.posX = -10.0;
	VerticalMazeitem1Col.posZ = 1.0;
	VerticalMazeitem1Col.sizeX = 1.0;
	VerticalMazeitem1Col.sizeZ = 6.0;
	rectangles.push_back(VerticalMazeitem1Col);

	auto VerticalMazeitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&VerticalMazeitem2->World, XMMatrixScaling(1.0f, 4.0f, 6.0f)* XMMatrixTranslation(10.0f, 4.1f, -5.0f));
	VerticalMazeitem2->ObjCBIndex = 79;
	VerticalMazeitem2->Mat = mMaterials["bush0"].get();
	VerticalMazeitem2->Geo = mGeometries["shapeGeo"].get();
	VerticalMazeitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	VerticalMazeitem2->IndexCount = VerticalMazeitem2->Geo->DrawArgs["box"].IndexCount;  //36
	VerticalMazeitem2->StartIndexLocation = VerticalMazeitem2->Geo->DrawArgs["box"].StartIndexLocation; //0
	VerticalMazeitem2->BaseVertexLocation = VerticalMazeitem2->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(VerticalMazeitem2));
	RenderCollisionRectangles VerticalMazeitem2Col;
	VerticalMazeitem2Col.posX = 10.0;
	VerticalMazeitem2Col.posZ = -5.0;
	VerticalMazeitem2Col.sizeX = 1.0;
	VerticalMazeitem2Col.sizeZ = 6.0;
	rectangles.push_back(VerticalMazeitem2Col);

	auto VerticalMazeitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&VerticalMazeitem3->World, XMMatrixScaling(1.0f, 4.0f,20.0f)* XMMatrixTranslation(18.0f, 4.1f, -23.0f));
	VerticalMazeitem3->ObjCBIndex = 80;
	VerticalMazeitem3->Mat = mMaterials["bush0"].get();
	VerticalMazeitem3->Geo = mGeometries["shapeGeo"].get();
	VerticalMazeitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	VerticalMazeitem3->IndexCount = VerticalMazeitem3->Geo->DrawArgs["box"].IndexCount;  //36
	VerticalMazeitem3->StartIndexLocation = VerticalMazeitem3->Geo->DrawArgs["box"].StartIndexLocation; //0
	VerticalMazeitem3->BaseVertexLocation = VerticalMazeitem3->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(VerticalMazeitem3));
	RenderCollisionRectangles VerticalMazeitem3Col;
	VerticalMazeitem3Col.posX = 18.0f;
	VerticalMazeitem3Col.posZ = -23.0f;
	VerticalMazeitem3Col.sizeX = 1.0;
	VerticalMazeitem3Col.sizeZ = 20.0;
	rectangles.push_back(VerticalMazeitem3Col);

	auto VerticalMazeitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&VerticalMazeitem4->World, XMMatrixScaling(1.0f, 4.0f, 8.0f)* XMMatrixTranslation(-18.0f, 4.1f, -18.0f));
	VerticalMazeitem4->ObjCBIndex = 81;
	VerticalMazeitem4->Mat = mMaterials["bush0"].get();
	VerticalMazeitem4->Geo = mGeometries["shapeGeo"].get();
	VerticalMazeitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	VerticalMazeitem4->IndexCount = VerticalMazeitem4->Geo->DrawArgs["box"].IndexCount;  //36
	VerticalMazeitem4->StartIndexLocation = VerticalMazeitem4->Geo->DrawArgs["box"].StartIndexLocation; //0
	VerticalMazeitem4->BaseVertexLocation = VerticalMazeitem4->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(VerticalMazeitem4));
	RenderCollisionRectangles VerticalMazeitem4Col;
	VerticalMazeitem4Col.posX = -18.0;
	VerticalMazeitem4Col.posZ = -18.0;
	VerticalMazeitem4Col.sizeX = 1.0;
	VerticalMazeitem4Col.sizeZ = 8.0;
	rectangles.push_back(VerticalMazeitem4Col);

	auto VerticalMazeitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&VerticalMazeitem5->World, XMMatrixScaling(1.0f, 4.0f, 6.0f)* XMMatrixTranslation(-18.0f, 4.1f, -30.0f));
	VerticalMazeitem5->ObjCBIndex = 82;
	VerticalMazeitem5->Mat = mMaterials["bush0"].get();
	VerticalMazeitem5->Geo = mGeometries["shapeGeo"].get();
	VerticalMazeitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	VerticalMazeitem5->IndexCount = VerticalMazeitem5->Geo->DrawArgs["box"].IndexCount;  //36
	VerticalMazeitem5->StartIndexLocation = VerticalMazeitem5->Geo->DrawArgs["box"].StartIndexLocation; //0
	VerticalMazeitem5->BaseVertexLocation = VerticalMazeitem5->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(VerticalMazeitem5));
	RenderCollisionRectangles VerticalMazeitem5Col;
	VerticalMazeitem5Col.posX = -18.0;
	VerticalMazeitem5Col.posZ = -30;
	VerticalMazeitem5Col.sizeX = 1.0;
	VerticalMazeitem5Col.sizeZ = 6.0;
	rectangles.push_back(VerticalMazeitem5Col);

	//
	auto HorizonMazeitem8 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem8->World, XMMatrixScaling(30.0f, 4.0f, 1.0f)* XMMatrixTranslation(3.5f, 4.1f, -13.0f));
	HorizonMazeitem8->ObjCBIndex = 83;
	HorizonMazeitem8->Mat = mMaterials["bush0"].get();
	HorizonMazeitem8->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem8->IndexCount = HorizonMazeitem8->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem8->StartIndexLocation = HorizonMazeitem8->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem8->BaseVertexLocation = HorizonMazeitem8->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem8));
	RenderCollisionRectangles HorizonMazeitem8Col;
	HorizonMazeitem8Col.posX = 3.5;
	HorizonMazeitem8Col.posZ = -13.0;
	HorizonMazeitem8Col.sizeX = 30.0;
	HorizonMazeitem8Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem8Col);

	auto HorizonMazeitem9 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem9->World, XMMatrixScaling(8.0f, 4.0f, 1.0f)* XMMatrixTranslation(10.0f, 4.1f, -26.0f));
	HorizonMazeitem9->ObjCBIndex = 84;
	HorizonMazeitem9->Mat = mMaterials["bush0"].get();
	HorizonMazeitem9->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem9->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem9->IndexCount = HorizonMazeitem9->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem9->StartIndexLocation = HorizonMazeitem9->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem9->BaseVertexLocation = HorizonMazeitem9->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem9));
	RenderCollisionRectangles HorizonMazeitem9Col;
	HorizonMazeitem9Col.posX = 10.0;
	HorizonMazeitem9Col.posZ = -26.0;
	HorizonMazeitem9Col.sizeX = 8.0;
	HorizonMazeitem9Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem9Col);

	auto HorizonMazeitem10 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem10->World, XMMatrixScaling(12.0f, 4.0f, 1.0f)* XMMatrixTranslation(-12.0f, 4.1f, -26.5f));
	HorizonMazeitem10->ObjCBIndex = 85;
	HorizonMazeitem10->Mat = mMaterials["bush0"].get();
	HorizonMazeitem10->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem10->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem10->IndexCount = HorizonMazeitem10->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem10->StartIndexLocation = HorizonMazeitem10->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem10->BaseVertexLocation = HorizonMazeitem10->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem10));
	RenderCollisionRectangles HorizonMazeitem10Col;
	HorizonMazeitem10Col.posX = -12.0;
	HorizonMazeitem10Col.posZ = -26.5;
	HorizonMazeitem10Col.sizeX = 12.0;
	HorizonMazeitem10Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem10Col);

	auto HorizonMazeitem11 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&HorizonMazeitem11->World, XMMatrixScaling(8.0f, 4.0f, 1.0f)* XMMatrixTranslation(-14.0f, 4.1f, -21.5f));
	HorizonMazeitem11->ObjCBIndex = 86;
	HorizonMazeitem11->Mat = mMaterials["bush0"].get();
	HorizonMazeitem11->Geo = mGeometries["shapeGeo"].get();
	HorizonMazeitem11->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	HorizonMazeitem11->IndexCount = HorizonMazeitem11->Geo->DrawArgs["box"].IndexCount;  //36
	HorizonMazeitem11->StartIndexLocation = HorizonMazeitem11->Geo->DrawArgs["box"].StartIndexLocation; //0
	HorizonMazeitem11->BaseVertexLocation = HorizonMazeitem11->Geo->DrawArgs["box"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(HorizonMazeitem11));
	RenderCollisionRectangles HorizonMazeitem11Col;
	HorizonMazeitem11Col.posX = -14.0;
	HorizonMazeitem11Col.posZ = -21.5;
	HorizonMazeitem11Col.sizeX = 8.0;
	HorizonMazeitem11Col.sizeZ = 1.0;
	rectangles.push_back(HorizonMazeitem11Col);

	RenderCollisionRectangles LeftBoundryCol;
	LeftBoundryCol.posX = -30.0;
	LeftBoundryCol.posZ = 0.0;
	LeftBoundryCol.sizeX = 2.0;
	LeftBoundryCol.sizeZ = 120.0;
	rectangles.push_back(LeftBoundryCol);

	RenderCollisionRectangles RightBoundryCol;
	RightBoundryCol.posX = 30.0;
	RightBoundryCol.posZ = 0.0;
	RightBoundryCol.sizeX = 2.0;
	RightBoundryCol.sizeZ = 120.0;
	rectangles.push_back(RightBoundryCol);

	RenderCollisionRectangles FrontBoundryCol;
	FrontBoundryCol.posX = 0.0;
	FrontBoundryCol.posZ = -56.0;
	FrontBoundryCol.sizeX = 60.0;
	FrontBoundryCol.sizeZ = 2.0;
	rectangles.push_back(FrontBoundryCol);

	RenderCollisionRectangles BackBoundryCol;
	BackBoundryCol.posX = 0.0;
	BackBoundryCol.posZ = 60.0;
	BackBoundryCol.sizeX = 58.0;
	BackBoundryCol.sizeZ = 2.0;
	rectangles.push_back(BackBoundryCol);

	// 
	// 
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

void ShapesApp::AABBAABBCollision(const GameTimer& gt)
{
	for (auto it = rectangles.begin(); it != rectangles.end(); it++)
	{
		float minimumTransX = MinimumTranslationVector1D(mCamera.GetPosition3f().x, mCamera.mSize.x / 2, it->posX, it->sizeX / 2);

		float minimumTransZ = MinimumTranslationVector1D(mCamera.GetPosition3f().z, mCamera.mSize.y / 2, it->posZ, it->sizeZ / 2);

		if (minimumTransX == 0 && minimumTransZ == 0) //
		{
			continue;
		}
		else if (minimumTransX != 0 && minimumTransZ != 0)
		{
			DirectX::XMFLOAT3 minimumTranslation;

			if (abs(minimumTransX) < abs(minimumTransZ)) // if the amount we would need to move them by the smaller in the x direction
			{
				// move along x because it's less effort (minimum translation)
				minimumTranslation.x = minimumTransX;
				minimumTranslation.z = 0;
				minimumTranslation.y = 0;

				mCamera.SetPosition(mCamera.GetPosition3f().x + minimumTranslation.x, mCamera.GetPosition3f().y, mCamera.GetPosition3f().z);
			}
			else
			{
				minimumTranslation.x = 0;
				minimumTranslation.z = minimumTransZ;
				minimumTranslation.y = 0;

				mCamera.SetPosition(mCamera.GetPosition3f().x, mCamera.GetPosition3f().y, mCamera.GetPosition3f().z + minimumTranslation.z);
			}
		}
	}
}

float ShapesApp::MinimumTranslationVector1D(const float centerA, const float radiusA, const float centerB, const float radiusB)
{
	// Vector between the objects from centerA to centerB
	float displacementAtoB = centerB - centerA;

	// Find distance
	float distance = abs(displacementAtoB);

	// Detect overlapping
	float radiiSum = radiusA + radiusB;
	float overlap = distance - radiiSum;

	if (overlap > 0)
	{
		return 0.0f;
	}

	// Are overlapping
	float directionAtoB = Sign(displacementAtoB);
	float minimumTranslationVector = directionAtoB * overlap;

	return minimumTranslationVector;
}

float ShapesApp::Sign(const float value)
{
	return (value < 0.0f) ? -1.0f : 1.0f;
}