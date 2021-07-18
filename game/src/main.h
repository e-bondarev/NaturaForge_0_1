#include <engine/engine.h>
#include "world/map/map.h"

using namespace Engine;
using namespace Game;

struct UBOScene
{
	glm::mat4x4 projection;
};

struct UBOInstance
{
	Aligned<glm::mat4x4> model;

	UBOInstance(uint32_t amount_of_instances) : model { amount_of_instances }
	{

	}
};

class NaturaForge : public App
{
public:
	int amountOfInstances = 1;

	glm::vec2 viewPosition { 0 };
	glm::vec2 lastViewPosition { 0 };

	void Init() override;
	void Update() override;
	void Shutdown() override;

private:
	std::vector<glm::vec4> renderData;

	void UpdateMap();
	void UpdateProjectionViewMatrix();
	void UpdateUBO();
	void RecordCommandBuffer(Vk::CommandPool* command_pool, Vk::CommandBuffer* cmd);
	void Render(Vk::CommandBuffer* cmd);
	void Present();

	Map* map;

	Vk::FrameManager* frameManager;
	std::vector<Vk::CommandPool*> commandPools;
	std::vector<Vk::CommandBuffer*> commandBuffers;
	std::vector<VkFence> imagesInFlight;

	struct
	{
		struct 
		{
			Vk::Buffer* vertexBuffer;
			Vk::Buffer* indexBuffer;
		} mesh;
		
		Vk::Buffer* dynamicVertexBuffer;

		struct
		{
			struct 
			{
				Vk::Buffer* buffer;
				UBOScene data;
			} perScene;
		} ubo;

		Vk::Pipeline* pipeline;		
	} scene;

	Vk::Image* image;
	Vk::ImageView* imageView;

	Vk::DescriptorPool* descriptorPool;

	Vk::DescriptorSetLayout* descriptorSetLayout;
	Vk::DescriptorSet* descriptorSet;
};

App* ::Engine::GetApp()
{
	return new NaturaForge();
}