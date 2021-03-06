#include "SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtx/transform.hpp>

#include <vector>
#include <chrono>

struct BlasInstanceData
{
	uint32_t meshIndex;
	uint32_t materialIndex;
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

void loadSceneGraph(const RaytracingDevice* device, const aiScene* scene, Scene& representation)
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

				if (mesh->HasTextureCoords(0))
				{
					aiVector3D texCoords = mesh->mTextureCoords[0][i];
					vertexMemory[i].texCoords = { texCoords.x, texCoords.y };
				}
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

			representation.meshBuffers.push_back({
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

aiReturn loadMaterialTexture(const aiScene* scene, const aiMaterial* material, aiTextureType textureType, int index, std::shared_ptr<uint8_t>& output, int& width, int& height)
{
	aiReturn error = AI_SUCCESS;

	//Get texture path
	aiString name;
	if ((error = material->Get(AI_MATKEY_TEXTURE(textureType, index), name)) != AI_SUCCESS)
	{
		//TODO: Support materials that don't use textures
		return error;
	}

	//TODO: Load duplicate textures only once

	//Load texture data
	const aiTexture* texture = nullptr;
	if (texture = scene->GetEmbeddedTexture(name.C_Str()))
	{
		if (texture->mHeight == 0) //Texture is compressed
		{
			int numChannels;

			//STB will try to detect the image type automatically so, there is no need to use 'texture->achFormatHint'
			stbi_uc* imageMemory = stbi_load_from_memory((const stbi_uc*)texture->pcData, texture->mWidth, &width, &height, &numChannels, 4);

			if (!imageMemory)
			{
				std::cerr << "Unable to load texture (format='" << texture->achFormatHint << "', index=" << index << ") from material '" << material->GetName().C_Str() <<"'" << std::endl;
				return AI_FAILURE;//TODO: Load a blank texture instead of crashing
			}

			output = std::shared_ptr<uint8_t>((uint8_t*)imageMemory, stbi_image_free);
		}
		else
		{
			width = texture->mWidth;
			height = texture->mHeight;

			size_t textureSize = 4 * width * height;
			output = std::shared_ptr<uint8_t>((uint8_t*)malloc(textureSize), free);

			memcpy(output.get(), texture->pcData, textureSize);
		}
	}
	else
	{
		assert(false);//TODO: Support non-embedded textures
	}

	return AI_SUCCESS;
}

void loadMaterials(const RaytracingDevice* device, const aiScene* scene, Scene& representation)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	std::vector<Buffer> stagingBuffers;

	device->getRenderDevice()->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			//Load image data and 
			int width;
			int height;
			std::shared_ptr<uint8_t> albedoData = nullptr;

			if (loadMaterialTexture(scene, scene->mMaterials[i], aiTextureType_DIFFUSE, 0, albedoData, width, height) != AI_SUCCESS)
			{
				PAUSE_AND_EXIT(-1);
			}

			VkDeviceSize imageSize = 4 * (VkDeviceSize)width * height;

			Buffer stagingBuffer = renderDevice->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			void* memory = nullptr;
			VK_CHECK(vkMapMemory(renderDevice->getDevice(), stagingBuffer.memory, 0, imageSize, 0, &memory));

			memcpy(memory, albedoData.get(), imageSize);

			vkUnmapMemory(renderDevice->getDevice(), stagingBuffer.memory);

			stagingBuffers.push_back(stagingBuffer);

			Image albedoTexture = renderDevice->createImage2D(width, height, VK_FORMAT_R8G8B8A8_UNORM, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			//Create texture sampler
			VkSamplerCreateInfo samplerCI = {};
			samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCI.mipLodBias = 0.0f;
			samplerCI.anisotropyEnable = VK_FALSE;
			samplerCI.maxAnisotropy = 1.0f;
			samplerCI.compareEnable = VK_FALSE;
			samplerCI.compareOp = VK_COMPARE_OP_NEVER;
			samplerCI.minLod = 0.0f;
			samplerCI.maxLod = 1.0f;
			samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerCI.unnormalizedCoordinates = VK_FALSE;

			VkSampler albedoSampler;
			VK_CHECK(vkCreateSampler(renderDevice->getDevice(), &samplerCI, nullptr, &albedoSampler));

			//Record buffer-to-image copy commands
			VkImageMemoryBarrier imageBarrier = {};
			imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarrier.srcAccessMask = 0;
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = albedoTexture.image;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount = 1;
			imageBarrier.subresourceRange.baseMipLevel = 0;
			imageBarrier.subresourceRange.levelCount = 1;

			vkCmdPipelineBarrier(commandBuffers[0],
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

			VkBufferImageCopy region = {};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

			vkCmdCopyBufferToImage(commandBuffers[0], stagingBuffer.buffer, albedoTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = 0;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			vkCmdPipelineBarrier(commandBuffers[0],
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

			//Add material to scene
			Material material;

			material.albedoIndex = (uint32_t)representation.textures.size();
			representation.textures.push_back(std::make_pair(albedoTexture, albedoSampler));

			representation.materials.push_back(material);
		}
	});

	for (size_t i = 0; i < stagingBuffers.size(); ++i)
	{
		renderDevice->destroyBuffer(stagingBuffers[i]);
	}
}

void createSceneDescriptorSets(const RaytracingDevice* raytracingDevice, Scene& scene)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding layoutBinding[] = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)scene.meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)scene.meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)scene.materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
	};

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.pBindings = layoutBinding;
	layoutCI.bindingCount = sizeof(layoutBinding) / sizeof(layoutBinding[0]);
	
	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &scene.descriptorSetLayout));

	//Create descriptor pool
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * (uint32_t)scene.meshBuffers.size() },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)scene.materials.size() }
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
	VkWriteDescriptorSet setWrites[4] = {};

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
	std::vector<VkDescriptorBufferInfo> vertexSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), vertexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer.buffer, val.vertexOffset, val.vertexSize }; });

	setWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[1].dstSet = scene.descriptorSet;
	setWrites[1].dstBinding = 1;
	setWrites[1].descriptorCount = (uint32_t)vertexSetWrites.size();
	setWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setWrites[1].pBufferInfo = vertexSetWrites.data();

	//Write index buffers (binding = 2)
	std::vector<VkDescriptorBufferInfo> indexSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), indexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.indexBuffer.buffer, val.indexOffset, val.indexSize }; });

	setWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[2].dstSet = scene.descriptorSet;
	setWrites[2].dstBinding = 2;
	setWrites[2].descriptorCount = (uint32_t)indexSetWrites.size();
	setWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	setWrites[2].pBufferInfo = indexSetWrites.data();

	//Write textures (binding = 3)
	std::vector<VkDescriptorImageInfo> imageSetWrites(scene.materials.size());
	std::transform(scene.textures.begin(), scene.textures.end(), imageSetWrites.begin(), [](const std::pair<Image, VkSampler>& texture)
		{ return VkDescriptorImageInfo{ texture.second, texture.first.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }; });

	setWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites[3].dstSet = scene.descriptorSet;
	setWrites[3].dstBinding = 3;
	setWrites[3].descriptorCount = (uint32_t)imageSetWrites.size();
	setWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	setWrites[3].pImageInfo = imageSetWrites.data();

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

	std::shared_ptr<Scene> representation = std::make_shared<Scene>();
	representation->device = device;

	//Load scene graph (meshes)
	loadSceneGraph(device, scene, *representation);

	//Load materials
	loadMaterials(device, scene, *representation);

	//Create scene descriptor sets
	createSceneDescriptorSets(device, *representation);

	importer.FreeScene();
	
	return representation;
};

Scene::~Scene()
{
	VkDevice deviceHandle = device->getRenderDevice()->getDevice();

	//Destroy acceleration structures
	tlas.destroy();

	for (BottomLevelAS& blas : blasList)
	{
		blas.destroy();
	}

	//Destroy buffers
	for (MeshBuffers& buffers : meshBuffers)
	{
		device->getRenderDevice()->destroyBuffer(buffers.vertexBuffer);
		device->getRenderDevice()->destroyBuffer(buffers.indexBuffer);
	}

	//Destroy texutres
	for (const std::pair<Image, VkSampler>& texture : textures)
	{
		device->getRenderDevice()->destroyImage(texture.first);

		vkDestroySampler(deviceHandle, texture.second, nullptr);
	}

	//Destroy descriptors
	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr);
	}

	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);
	}
}