#pragma once
#include "Game/GameObject.h"

class CameraComponent : public Component
{
	struct CameraConstantBuffer
	{
		DirectX::SimpleMath::Matrix View{ DirectX::SimpleMath::Matrix::Identity };
		DirectX::SimpleMath::Vector3 Position{ DirectX::SimpleMath::Vector3::Zero };
		float FOV{ 0.0f };
	};

public:
	CameraComponent();
	CameraComponent(float fov);
	CameraComponent(const CameraComponent&) = default;
	CameraComponent(CameraComponent&&) = default;
	CameraComponent& operator=(const CameraComponent&) = default;
	CameraComponent& operator=(CameraComponent&&) = default;
	~CameraComponent() override = default;

	void Update(float deltaTime) override {};
	void Render() override;
	void RenderGUI() override;

	[[nodiscard]] float GetFOV() const { return FOV; }
	void SetFOV(const float val) { FOV = val; }

	[[nodiscard]] DirectX::SimpleMath::Matrix GetViewMatrix() const;

protected:
	[[nodiscard]] std::string GetComponentName() const override { return "Camera"; }

private:
	void Initialise();
	void LoadSkyboxFromPath(const std::wstring& path);

	float FOV{ 0.0f };

	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer{};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SkyboxSRV{};
	Microsoft::WRL::ComPtr<ID3D11SamplerState> LinearSampler{};
};
