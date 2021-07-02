#include "Camera.h"

#include "api/RaytracingDevice.h"

#include <glm/gtx/quaternion.hpp>

class PerspectiveCamera : public Camera
{
private:
	struct Data
	{
		glm::mat3 viewMatrix;
		glm::vec3 position;
	};

	Buffer m_dataBuffer;
	void* m_dataPtr = nullptr;

	float m_fov;

	const RenderDevice* m_device = nullptr;
public:
	PerspectiveCamera(Window* window) :
		Camera(window, CameraType::PERSPECTIVE),
		m_fov(70.0f) {}

	void init(const RenderDevice* device) override
	{
		m_device = device;

		m_dataBuffer = m_device->createBuffer(sizeof(Data), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		vkMapMemory(m_device->getDevice(), m_dataBuffer.memory, 0, sizeof(Data), 0, &m_dataPtr);
	}

	void onUpdated(glm::vec3 position, glm::quat rotation, int width, int heigth) override
	{
		Data* data = (Data*)m_dataPtr;
		
		float tanHalfFOV = tan(m_fov / 2.0f);
		float aspectRatio = (float)width / heigth;

		glm::mat3 projectionMatrix(glm::vec3(2.0f, 0.0f, 0.0f),
								   glm::vec3(0.0f, 2.0f, 0.0f),
								   glm::vec3(-1.0f, -1.0f, 1.0f));
	
		projectionMatrix = glm::mat3(glm::vec3(tanHalfFOV * aspectRatio, 0.0f, 0.0f),
									 glm::vec3(0.0f, -tanHalfFOV, 0.0f),
									 glm::vec3(0.0f, 0.0f, 1.0f)) * projectionMatrix;

		projectionMatrix = glm::toMat3(rotation) * projectionMatrix;
	
		data->viewMatrix = projectionMatrix;
		data->position = position;
	}

	VkDescriptorBufferInfo getDescriptorInfo() const override
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = m_dataBuffer.buffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(Data);

		return bufferInfo;
	}

	std::string getCameraDefintions() const override
	{
		return "#include \"common/camera_perspective.glsl\"";
	}

	void destroy() override
	{
		vkUnmapMemory(m_device->getDevice(), m_dataBuffer.memory);
		m_device->destroyBuffer(m_dataBuffer);
	}
};