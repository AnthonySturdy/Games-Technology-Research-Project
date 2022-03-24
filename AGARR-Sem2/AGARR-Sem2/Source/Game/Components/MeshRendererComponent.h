#pragma once
#include "Game/GameObject.h"
#include "Rendering/Mesh.h"
#include "Rendering/Shader.h"

class MeshRendererComponent : public Component
{
	struct PerObjectConstantBuffer
	{
		DirectX::SimpleMath::Matrix World{ DirectX::SimpleMath::Matrix::Identity };
	};

public:
	MeshRendererComponent();
	MeshRendererComponent(const MeshRendererComponent&) = default;
	MeshRendererComponent(MeshRendererComponent&&) = default;
	MeshRendererComponent& operator=(const MeshRendererComponent&) = default;
	MeshRendererComponent& operator=(MeshRendererComponent&&) = default;
	~MeshRendererComponent() override = default;

	void Update(float deltaTime) override;
	void Render() override;
	void RenderGUI() override;

protected:
	[[nodiscard]] std::string GetComponentName() const override { return "Mesh Renderer"; }

private:
	void CreateConstantBuffer();

	std::shared_ptr<Mesh> MeshData{ nullptr };
	std::shared_ptr<Shader> MeshShader{ nullptr };

	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
};
