#include "SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <glm/gtx/transform.hpp>

#include <vector>
#include <chrono>

struct BlasInstanceData
{
	uint32_t meshIndex;
	uint32_t materialIndex;
};

struct MeshBuffers
{
	Buffer vertexBuffer;
	VkDeviceSize vertexOffset;
	VkDeviceSize vertexSize;

	Buffer indexBuffer;
	VkDeviceSize indexOffset;
	VkDeviceSize indexSize;
};

void parseSceneGraphNode(const RaytracingDevice* device, std::vector<VkAccelerationStructureInstanceKHR>& instances, const std::vector<BottomLevelAS>& blasList,
						 aiNode* node, glm::mat4 transform, std::vector<BlasInstanceData>& instanceData)
{
	glm::mat4 nodeTransform = {
		{ node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4 },
		{ node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4 },
		{ node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4 },
		{ node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3, node->mTransformation.d4 }
	};

	transform *= nodeTransform;

	//Add BLAS instances to TLAS
	for (unsigned int i = 0; i < node->mNumMeshes; ++i)
	{
		instances.push_back(device->compileInstances(blasList[node->mMeshes[i]], transform, node->mMeshes[i]/*gl_InstanceCustomIndexEXT*/, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR));

		instanceData.push_back({});
	}

	//Traverse children
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		parseSceneGraphNode(device, instances, blasList, node->mChildren[i], transform, instanceData);
	}
}

void loadSceneGraph(const RaytracingDevice* device, const aiScene* scene, Scene& representation, std::vector<MeshBuffers>& meshBuffers)
{
	//Parse meshes
	std::vector<BottomLevelAS> meshBLASList(scene->mNumMeshes);
	std::vector<Buffer> stagingBuffers(scene->mNumMeshes);

	device->getRenderDevice()->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			const aiMesh* mesh = scene->mMeshes[i];

			VkDeviceSize vertexBufferSize = mesh->mNumVertices * sizeof(MeshVertex);
			VkDeviceSize indexBufferSize = mesh->mNumFaces * (3 * sizeof(uint32_t));
			VkDeviceSize stagingBufferSize = vertexBufferSize + indexBufferSize;

			//Create staging buffers
			Buffer stagingBuffer = device->getRenderDevice()->createBuffer(stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			void* memory = nullptr;
			VK_CHECK(vkMapMemory(device->getRenderDevice()->getDevice(), stagingBuffer.memory, 0, stagingBufferSize, 0, &memory));

			MeshVertex* vertexMemory = (MeshVertex*)memory;
			for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D pos = mesh->mVertices[i];
				vertexMemory[i].position = { pos.x, pos.y, pos.z };
				
				aiVector3D normal = mesh->mNormals[i];
				vertexMemory[i].normal = { normal.x, normal.y, normal.z };
			}

			unsigned int* indexMemory = (unsigned int*)&vertexMemory[mesh->mNumVertices];
			for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
			{
				assert(mesh->mFaces[i].mNumIndices == 3);

				indexMemory[3 * i] = mesh->mFaces[i].mIndices[0];
				indexMemory[3 * i + 1] = mesh->mFaces[i].mIndices[1];
				indexMemory[3 * i + 2] = mesh->mFaces[i].mIndices[2];
			}

			vkUnmapMemory(device->getRenderDevice()->getDevice(), stagingBuffer.memory);

			//Create BLAS for mesh
			Buffer vertexBuffer = device->getRenderDevice()->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			Buffer indexBuffer = device->getRenderDevice()->createBuffer(indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																						  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			std::shared_ptr<const BLASGeometryInfo> geomertryInfo = device->compileGeometry(vertexBuffer, sizeof(MeshVertex), mesh->mNumVertices, indexBuffer, mesh->mNumFaces, { 0 });

			meshBLASList[i].init(device, geomertryInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

			stagingBuffers[i] = stagingBuffer;

			meshBuffers.push_back({
				vertexBuffer, 0, vertexBufferSize,
				indexBuffer, 0, indexBufferSize
			});

			//Record copy commands
			VkBufferCopy copyRegion = { 0, 0, vertexBufferSize };
			vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, vertexBuffer.buffer, 1, &copyRegion);

			copyRegion = { vertexBufferSize, 0, indexBufferSize };
			vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, indexBuffer.buffer, 1, &copyRegion);
		}
	});

	//Free staging buffers
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		device->getRenderDevice()->destroyBuffer(stagingBuffers[i]);
	}

	device->buildBLAS(meshBLASList);

	//Parse scene graph
	std::vector<VkAccelerationStructureInstanceKHR> accelStructInstances;
	std::vector<BlasInstanceData> instanceData;

	parseSceneGraphNode(device, accelStructInstances, meshBLASList, scene->mRootNode, glm::identity<glm::mat4>(), instanceData);

	//Build TLAS
	TopLevelAS tlas;
	tlas.init(device);

	device->buildTLAS(tlas, accelStructInstances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	representation.blasList = std::move(meshBLASList);
	representation.tlas = std::move(tlas);
}

void createSceneDescriptorSets(const RaytracingDevice* raytracingDevice, const std::vector<MeshBuffers>& meshBuffers, Scene& scene)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding layoutBinding[] = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
	};

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.pBindings = layoutBinding;
	layoutCI.bindingCount = sizeof(layoutBinding) / sizeof(layoutBinding[0]);
	
	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &scene.descriptorSetLayout));

	//Create descriptor pool
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * (uint32_t)meshBuffers.size() }
	};

	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.poolSizeCount = sizeof(descPoolSizes) / sizeof(descPoolSizes[0]);
	descPoolCI.pPoolSizes = descPoolSizes;
	descPoolCI.maxSets = 1;

	VK_CHECK(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &scene.descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = scene.descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &scene.descriptorSetLayout;

	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &scene.descriptorSet));

	//Update scene descriptor
	VkWriteDescriptorSet setWrites[3] = {};

	//Write TLAS (binding = 0)
	VkAccelerationStructureKHR tlas = scene.tlas.get();

	VkWriteDescriptorSetAccelerationStructureKHR tlasSetWrite = {};
	tlasSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	tlasSetWrite.accelerationStructureCount = 1;
	tlasSetWrite.pAccelerationStructures = &tlas;

	setWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[0].pNext = &tlasSetWrite;
	setWrites[0].dstSet = scene.descriptorSet;
	setWrites[0].dstBinding = 0;
	setWrites[0].descriptorCount = 1;
	setWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	//Write vertex buffers (binding = 1)
	std::vector<VkDescriptorBufferInfo> vertexSetWrites(meshBuffers.size());
	std::transform(meshBuffers.begin(), meshBuffers.end(), vertexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer.buffer, val.vertexOffset, val.vertexSize }; });

	setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[1].dstSet = scene.descriptorSet;
	setWrites[1].dstBinding = 1;
	setWrites[1].descriptorCount = (uint32_t)vertexSetWrites.size();
	setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setWrites[1].pBufferInfo = vertexSetWrites.data();

	//Write index buffers (binding = 1)
	std::vector<VkDescriptorBufferInfo> indexSetWrites(meshBuffers.size());
	std::transform(meshBuffers.begin(), meshBuffers.end(), indexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.indexBuffer.buffer, val.indexOffset, val.indexSize }; });

	setWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[2].dstSet = scene.descriptorSet;
	setWrites[2].dstBinding = 2;
	setWrites[2].descriptorCount = (uint32_t)indexSetWrites.size();
	setWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setWrites[2].pBufferInfo = indexSetWrites.data();

	vkUpdateDescriptorSets(device, sizeof(setWrites) / sizeof(setWrites[0]), setWrites, 0, nullptr);
}

std::shared_ptr<Scene> SceneLoader::loadScene(const RaytracingDevice* device, const char* scenePath)
{
	//Import scene from file
	std::cout << "Importing scene " << scenePath << "... ";

	auto start = std::chrono::high_resolution_clock::now();

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(scenePath, aiProcessPreset_TargetRealtime_MaxQuality);

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;

	std::vector<MeshBuffers> meshBuffers;

	std::shared_ptr<Scene> representation = std::make_shared<Scene>();
	representation->device = device;

	//Load scene graph (meshes)
	loadSceneGraph(device, scene, *representation, meshBuffers);

	//Create scene descriptor sets
	createSceneDescriptorSets(device, meshBuffers, *representation);

	importer.FreeScene();
	
	return representation;
};

Scene::~Scene()
{
	tlas.destroy();

	for (BottomLevelAS& blas : blasList)
	{
		blas.destroy();

		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->vertexBuffer);
		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->indexBuffer);
	}

	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->getRenderDevice()->getDevice(), descriptorSetLayout, nullptr);
	}

	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->getRenderDevice()->getDevice(), descriptorPool, nullptr);
	}
}