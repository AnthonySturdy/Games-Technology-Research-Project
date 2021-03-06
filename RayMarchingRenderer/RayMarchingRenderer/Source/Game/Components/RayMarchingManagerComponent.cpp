#include "pch.h"
#include "RayMarchingManagerComponent.h"

#include "MaterialComponent.h"
#include "MeshRendererComponent.h"
#include "RayMarchLightComponent.h"
#include "SDFManagerComponent.h"
#include "TransformComponent.h"

RayMarchingManagerComponent::RayMarchingManagerComponent(const std::vector<GameObject*>& gameObjects)
	: GameObjects(gameObjects)
{
	CreateConstantBuffers();
}

void RayMarchingManagerComponent::CreateConstantBuffers()
{
	const auto device = DX::DeviceResources::Instance()->GetD3DDevice();

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(RenderSettings);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	DX::ThrowIfFailed(device->CreateBuffer(&bd, nullptr, RenderSettingsConstantBuffer.ReleaseAndGetAddressOf()));

	bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(RayMarchScene);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	DX::ThrowIfFailed(device->CreateBuffer(&bd, nullptr, RayMarchSceneConstantBuffer.ReleaseAndGetAddressOf()));

	bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(RayMarchLights);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	DX::ThrowIfFailed(device->CreateBuffer(&bd, nullptr, RayMarchLightConstantBuffer.ReleaseAndGetAddressOf()));
}

void RayMarchingManagerComponent::Update(float deltaTime)
{
	// Generate scene distance shader
	const auto rmObjects = GameObject::FindComponents<RayMarchObjectComponent>(GameObjects);

	const auto sdfManager = Parent->GetComponent<SDFManagerComponent>();

	// Construct string of signed distance functions being used and total boolean operator values
	std::string sdfs;
	int boolOpsTotal = 0;
	for (const auto& obj : rmObjects)
	{
		const std::string objSdf = sdfManager->GenerateSignedDistanceFunction(obj->GetSDFType());
		if (!sdfs.contains(objSdf))
			sdfs += objSdf;

		boolOpsTotal += obj->GetBoolOperator();
	};

	// Use hash to prevent unnecessary shader changes
	static constexpr std::hash<std::string> hash;
	static size_t prevSdfHash = hash("");
	const size_t curSdfHash = hash(sdfs + std::to_string(rmObjects.size()) + std::to_string(boolOpsTotal));
	if (curSdfHash != prevSdfHash)
	{
		prevSdfHash = curSdfHash;

		sdfManager->WriteStringToHeaderShader(sdfs);
		sdfManager->WriteSceneDistanceFunctionToShaderHeader(sdfManager->GenerateSceneDistanceFunctionContents(rmObjects));

		// Recompile pixel shader
		const auto meshRenderer = Parent->GetComponent<MeshRendererComponent>();
		const auto shader = meshRenderer->GetShader();
		shader->CreatePixelShader();
	}
}

void RayMarchingManagerComponent::Render()
{
	const auto context = DX::DeviceResources::Instance()->GetD3DDeviceContext();

	// Update RenderSettings constant buffer
	const auto viewportSize = DX::DeviceResources::Instance()->GetViewportSize();
	RenderSettingsData.Resolution[0] = viewportSize.right;
	RenderSettingsData.Resolution[1] = viewportSize.bottom;
	context->UpdateSubresource(RenderSettingsConstantBuffer.Get(), 0, nullptr, &RenderSettingsData, 0, 0);
	context->PSSetConstantBuffers(0, 1, RenderSettingsConstantBuffer.GetAddressOf());

	// Update R.M. Scene data constant buffer
	const auto rmObjects = GameObject::FindComponents<RayMarchObjectComponent>(GameObjects);
	for (int i = 0; i < RAYMARCH_MAX_OBJECTS; ++i)
	{
		// Populate GPU cbuffer with object data
		if (i < rmObjects.size())
		{
			const auto transform = rmObjects[i]->Parent->GetComponent<TransformComponent>();

			RayMarchSceneData.ObjectsList[i].Position = transform->GetPosition();
			RayMarchSceneData.ObjectsList[i].Rotation = transform->GetRotation();
			RayMarchSceneData.ObjectsList[i].Scale = transform->GetScale();

			RayMarchSceneData.ObjectsList[i].Parameters = rmObjects[i]->GetParameters();
			RayMarchSceneData.ObjectsList[i].SDFType = rmObjects[i]->GetSDFType();
			RayMarchSceneData.ObjectsList[i].BoolOperator = rmObjects[i]->GetBoolOperator();

			const auto material = rmObjects[i]->Parent->GetComponent<MaterialComponent>();
			RayMarchSceneData.ObjectsList[i].Colour = material->GetColour();
			RayMarchSceneData.ObjectsList[i].Metalicness = material->GetMetalicness();
			RayMarchSceneData.ObjectsList[i].Roughness = material->GetRoughness();
		}
		else
		{
			// Clear if object data not in use
			RayMarchSceneData.ObjectsList[i] = {};
		}
	}
	context->UpdateSubresource(RayMarchSceneConstantBuffer.Get(), 0, nullptr, &RayMarchSceneData, 0, 0);
	context->PSSetConstantBuffers(2, 1, RayMarchSceneConstantBuffer.GetAddressOf());

	// Update R.M. Lights data constant buffer
	const auto rmLights = GameObject::FindComponents<RayMarchLightComponent>(GameObjects);
	for (int i = 0; i < RAYMARCH_MAX_LIGHTS; ++i)
	{
		if (i < rmLights.size())
		{
			const auto transform = rmLights[i]->Parent->GetComponent<TransformComponent>();

			RayMarchLightData.LightsList[i].Position = transform->GetPosition();
			RayMarchLightData.LightsList[i].Colour = rmLights[i]->GetColour();
			RayMarchLightData.LightsList[i].ShadowSharpness = rmLights[i]->GetShadowSharpness();
			RayMarchLightData.LightsList[i].ConstantAttenuation = rmLights[i]->GetConstantAttenuation();
			RayMarchLightData.LightsList[i].LinearAttenuation = rmLights[i]->GetLinearAttenuation();
			RayMarchLightData.LightsList[i].QuadraticAttenuation = rmLights[i]->GetQuadraticAttenuation();
		}
		else
		{
			RayMarchLightData.LightsList[i] = {};
		}
	}
	context->UpdateSubresource(RayMarchLightConstantBuffer.Get(), 0, nullptr, &RayMarchLightData, 0, 0);
	context->PSSetConstantBuffers(3, 1, RayMarchLightConstantBuffer.GetAddressOf());
}

void RayMarchingManagerComponent::RenderGUI()
{
	ImGui::DragInt("Max Steps", reinterpret_cast<int*>(&RenderSettingsData.MaxSteps), 1.0f, 1, 1000);
	ImGui::DragFloat("Max Dist", &RenderSettingsData.MaxDist, .5f, 1.0f, 10000.0f);
	ImGui::DragFloat("Threshold", &RenderSettingsData.IntersectionThreshold, 0.0001f, 0.0001f, 0.3f);
	ImGui::DragFloat("AO Strength", &RenderSettingsData.AmbientOcclusionStrength, 0.001f, 0.005f, 10.0f);
}
