#pragma once

#include "ecs/ecs.h"

#include "../map/map.h"

#include "renderer/atlas/tile_map.h"
#include "color_pass.h"
#include "light_pass.h"
#include "composition.h"

#include "ecs/collections.h"

class MapRenderer : public Component, public Collections::IRenderable, public Collections::IOnUpdate
{
public:
	MapRenderer();
	~MapRenderer();
	void Init(Map* map, Camera* camera, Engine::Vk::DescriptorPool* descriptor_pool);

	void UpdateRenderData();
	void UpdateSpace();

	void GetRenderData(Map* map);

	virtual void Update() override;
	virtual void Render(Engine::Vk::Frame* frame) override;

	std::vector<Engine::Vk::CommandBuffer*>& GetCommandBuffers();
	Engine::Vk::CommandPool* GetCurrentCmdPool();
	Engine::Vk::CommandBuffer* GetCurrentCmdBuffer();

	void UpdateCmdBuffers();

private:
	Map* map;
	Camera* camera;

	bool initialized { false };
	bool updateCmdBuffers { true };
	
	std::vector<glm::vec4> blocks_to_render;
	std::vector<glm::vec4> walls_to_render;
	std::vector<glm::vec2> lights_to_render;

	std::vector<Engine::Vk::CommandPool*> commandPools;
	std::vector<Engine::Vk::CommandBuffer*> commandBuffers;
	Offscreen::ColorPass* colorPass;
	Offscreen::LightPass* lightPass;
	Composition* composition;

	MapRenderer(const MapRenderer&) = delete;
	MapRenderer& operator=(const MapRenderer&) = delete;
};