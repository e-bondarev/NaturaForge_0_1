#include <iostream>

#include <engine/engine.h>

using namespace Engine;

struct UBOScene
{
	glm::mat4x4 view;
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
	UBOScene uboScene;
	UBOInstance* uboInstance;

	int amountOfInstances = 4;

	void Init() override
	{
		uboScene.view = glm::mat4x4(1);
		uboScene.projection = glm::perspective(glm::radians(70.0f), window->GetSize().x / window->GetSize().y, 0.1f, 1000.0f);

		uboInstance = new UBOInstance(amountOfInstances);
		size_t buffer_size = amountOfInstances * Aligned<glm::mat4x4>::dynamicAlignment;

		frameManager = new Vk::FrameManager();

		for (int i = 0; i < Vk::Global::swapChain->GetImageViews().size(); i++)
		{
			Vk::CommandPool* new_command_pool = new Vk::CommandPool();
			commandPools.push_back(new_command_pool);
			commandBuffers.push_back(new Vk::CommandBuffer(new_command_pool));
		}

		std::vector<VkDescriptorSetLayoutBinding> bindings = 
		{
			Vk::CreateBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER),
			Vk::CreateBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
		};

		descriptorSetLayout = new Vk::DescriptorSetLayout(bindings);

		glm::vec2 viewport_size = { Vk::Global::swapChain->GetExtent().width, Vk::Global::swapChain->GetExtent().height };

		Assets::Text vs_code("assets/shaders/default.vert.spv");
		Assets::Text fs_code("assets/shaders/default.frag.spv");

		pipeline = new Vk::Pipeline(
			vs_code.GetContent(), fs_code.GetContent(), 
			viewport_size, Vk::Global::swapChain->GetImageFormat(), 
			Vk::Vertex::GetBindingDescriptions(), Vk::Vertex::GetAttributeDescriptions(),
			{ descriptorSetLayout->GetVkDescriptorSetLayout() }
		);

		{			
			const std::vector<Vk::Vertex> vertices = 
			{
				{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
				{{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
				{{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
				{{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
			};

			Vk::Buffer staging_buffer(vertices);
			mesh.vertexBuffer = new Vk::Buffer(&staging_buffer);
		}
		{
			const std::vector<uint16_t> indices = 
			{
				0, 1, 2, 2, 3, 0
			};

			Vk::Buffer staging_buffer(indices);
			mesh.indexBuffer = new Vk::Buffer(&staging_buffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		}

		descriptorPool = new Vk::DescriptorPool({
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 }
		});

		uboSceneBuffer = new Vk::Buffer(
			sizeof(UBOScene),
			1,
			&uboScene,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		);

		uboInstanceBuffer = new Vk::Buffer(
			buffer_size,
			1,
			uboInstance->model.data,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		);
		uboInstanceBuffer->SetDescriptor(Aligned<glm::mat4x4>::dynamicAlignment);

		descriptorSet = new Vk::DescriptorSet(descriptorPool, { descriptorSetLayout->GetVkDescriptorSetLayout() });
		std::vector<VkWriteDescriptorSet> write_descriptor_sets = 
		{
			Vk::CreateWriteDescriptorSet(descriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboSceneBuffer->GetDescriptor()),
			Vk::CreateWriteDescriptorSet(descriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &uboInstanceBuffer->GetDescriptor())
		};

		descriptorSet->Update(write_descriptor_sets);

		imagesInFlight.resize(Vk::Global::swapChain->GetImageViews().size());
		for (const VkImageView& image_view : Vk::Global::swapChain->GetImageViews())
			framebuffers.push_back(new Vk::Framebuffer(image_view, pipeline->GetRenderPass()->GetVkRenderPass(), viewport_size));
	}

	void UpdateUBO()
	{
		static float alpha = 0.0f;
		alpha += 0.0001f;

		uboInstance->model[0] = glm::translate(glm::mat4x4(1), glm::vec3(0 + alpha, 0, -10));
		uboInstance->model[1] = glm::translate(glm::mat4x4(1), glm::vec3(0 - alpha, 0, -10));
		uboInstance->model[2] = glm::translate(glm::mat4x4(1), glm::vec3(0, 0 - alpha, -10));
		uboInstance->model[3] = glm::translate(glm::mat4x4(1), glm::vec3(0, 0 + alpha, -10));

		uboInstanceBuffer->Update(uboInstance->model.data);
	}

	void RecordCommandBuffer(Vk::CommandPool* command_pool, Vk::CommandBuffer* cmd, Vk::Framebuffer* framebuffer)
	{
		command_pool->Reset();

		cmd->Begin();
			cmd->BeginRenderPass(pipeline->GetRenderPass(), framebuffer);
				cmd->BindPipeline(pipeline);
					cmd->BindVertexBuffers({ mesh.vertexBuffer }, { 0 });
					cmd->BindIndexBuffer(mesh.indexBuffer);					
						for (int i = 0; i < amountOfInstances; i++)
						{
							uint32_t dynamic_offset = i * Aligned<glm::mat4x4>::dynamicAlignment;
							cmd->BindDescriptorSets(pipeline, 1, &descriptorSet->GetVkDescriptorSet(), 1, &dynamic_offset);							
							cmd->DrawIndexed(mesh.indexBuffer->GetAmountOfElements(), 1, 0, 0, 0);
						}
			cmd->EndRenderPass();
		cmd->End();
	}

	void Render(Vk::CommandBuffer* cmd)
	{		
		Vk::Frame* current_frame = frameManager->GetCurrentFrame();

		vkResetFences(Vk::Global::device->GetVkDevice(), 1, &current_frame->GetInFlightFence());

		cmd->SubmitToQueue(
			Vk::Global::Queues::graphicsQueue, 
			&current_frame->GetImageAvailableSemaphore(), 
			&current_frame->GetRenderFinishedSemaphore(), 
			current_frame->GetInFlightFence()
		);
	}

	void Present()
	{
		VkResult result = Vk::Global::swapChain->Present(&frameManager->GetCurrentFrame()->GetRenderFinishedSemaphore(), 1);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) 
		{
			
		} 
		else if (result != VK_SUCCESS) 
		{
			THROW("Can't present.");
		}

		frameManager->NextFrame();
	}

	void Update() override
	{		
		Vk::Frame* current_frame = frameManager->GetCurrentFrame();

		uint32_t image_index = Vk::Global::swapChain->AcquireImage(current_frame->GetImageAvailableSemaphore());

		if (imagesInFlight[image_index] != VK_NULL_HANDLE) 
			vkWaitForFences(Vk::Global::device->GetVkDevice(), 1, &imagesInFlight[image_index], VK_TRUE, UINT64_MAX);

		vkWaitForFences(Vk::Global::device->GetVkDevice(), 1, &current_frame->GetInFlightFence(), VK_TRUE, UINT64_MAX);
		vkResetFences(Vk::Global::device->GetVkDevice(), 1, &current_frame->GetInFlightFence());

		imagesInFlight[image_index] = current_frame->GetInFlightFence();

		Vk::CommandPool* current_command_pool = commandPools[image_index];
		Vk::CommandBuffer* current_command_buffer = commandBuffers[image_index];	
		Vk::Framebuffer* current_framebuffer = framebuffers[image_index];

		UpdateUBO();
		RecordCommandBuffer(current_command_pool, current_command_buffer, current_framebuffer);
		Render(current_command_buffer);
		Present();
	}

	void Shutdown() override
	{
		Vk::Global::device->WaitIdle();

		delete uboInstance;

		delete descriptorSet;
		delete descriptorPool;
		delete descriptorSetLayout;
		delete uboSceneBuffer;
		delete uboInstanceBuffer;

		for (int i = 0; i < framebuffers.size(); i++)
			delete framebuffers[i];

		delete mesh.vertexBuffer;
		delete mesh.indexBuffer;

		delete pipeline;

		VK_ASSERT(commandBuffers.size() == commandPools.size());
		for (int i = 0; i < commandBuffers.size(); i++)
		{
			delete commandBuffers[i];
			delete commandPools[i];
		}

		delete frameManager;
	}

	Vk::FrameManager* frameManager;
	std::vector<Vk::CommandPool*> commandPools;
	std::vector<Vk::CommandBuffer*> commandBuffers;
	std::vector<VkFence> imagesInFlight;

	struct 
	{
		Vk::Buffer* vertexBuffer;
		Vk::Buffer* indexBuffer;
	} mesh;

	Vk::Buffer* uboSceneBuffer;
	Vk::Buffer* uboInstanceBuffer;

	Vk::Pipeline* pipeline;

	Vk::DescriptorPool* descriptorPool;
	Vk::DescriptorSet* descriptorSet;

	std::vector<Vk::Framebuffer*> framebuffers;

	Vk::DescriptorSetLayout* descriptorSetLayout;
};

App* ::Engine::GetApp()
{
	return new NaturaForge();
}

ENTRY()