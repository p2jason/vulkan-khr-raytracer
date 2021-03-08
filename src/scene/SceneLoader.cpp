#include "SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/gtx/transform.hpp>

#include <vector>
#include <chrono>

#define DESC_SET_WRITE_BUFFER(e, desc, bind, arr, type)	\
if (arr.size() > 0) {									\
	VkWriteDescriptorSet inf = {};						\
	inf.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;	\
	inf.dstSet = desc;									\
	inf.dstBinding = bind;								\
	inf.descriptorCount = (uint32_t)arr.size();			\
	inf.descriptorType = type;							\
	inf.pBufferInfo = arr.data();						\
	e.push_back(inf);									\
}

#define DESC_SET_WRITE_IMAGE(e, desc, bind, arr, type)	\
if (arr.size() > 0) {									\
	VkWriteDescriptorSet inf = {};						\
	inf.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;	\
	inf.dstSet = desc;									\
	inf.dstBinding = bind;								\
	inf.descriptorCount = (uint32_t)arr.size();			\
	inf.descriptorType = type;							\
	inf.pImageInfo = arr.data();						\
	e.push_back(inf);									\
}

void parseSceneGraphNode(const RaytracingDevice* device, std::vector<VkAccelerationStructureInstanceKHR>& instances, const std::vector<BottomLevelAS>& blasList,
						 const aiScene* scene, aiNode* node, glm::mat4 transform, std::vector<uint32_t>& materialIndices)
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

		materialIndices.push_back(scene->mMeshes[node->mMeshes[i]]->mMaterialIndex);
	}

	//Traverse children
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		parseSceneGraphNode(device, instances, blasList, scene, node->mChildren[i], transform, materialIndices);
	}
}

void loadSceneGraph(const RaytracingDevice* device, const aiScene* scene, Scene& representation, std::vector<uint32_t>& materialIndices)
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

			std::shared_ptr<const BLASGeometryInfo> geomertryInfo = device->compileGeometry(vertexBuffer, sizeof(MeshVertex), mesh->mNumVertices, indexBuffer, mesh->mNumFaces, { 0 }, 0);

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

	parseSceneGraphNode(device, accelStructInstances, meshBLASList, scene, scene->mRootNode, glm::identity<glm::mat4>(), materialIndices);

	//Build TLAS
	TopLevelAS tlas;
	tlas.init(device);

	device->buildTLAS(tlas, accelStructInstances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	representation.blasList = std::move(meshBLASList);
	representation.tlas = std::move(tlas);
}

bool loadMaterialTexture(const aiScene* scene, const aiMaterial* material, aiTextureType textureType, int index, std::shared_ptr<uint8_t>& output, int& width, int& height)
{
	//Get texture path
	aiString name;
	if (material->Get(AI_MATKEY_TEXTURE(textureType, index), name) != AI_SUCCESS)
	{
		//Material doesn't have the texture requested
		return false;
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
				return false;
			}

			output = std::shared_ptr<uint8_t>((uint8_t*)imageMemory, stbi_image_free);
		}
		else
		{
			width = texture->mWidth;
			height = texture->mHeight;

			size_t textureSize = 4 * (size_t)width * height;
			output = std::shared_ptr<uint8_t>((uint8_t*)malloc(textureSize), free);

			memcpy(output.get(), texture->pcData, textureSize);
		}
	}
	else
	{
		assert(false);//TODO: Support non-embedded textures
	}

	return true;
}

std::pair<Image, VkSampler> uploadTexture(const RaytracingDevice* device, VkCommandBuffer commandBuffer, std::shared_ptr<uint8_t> data, int width, int height, std::vector<Buffer>& stagingBuffers)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	VkDeviceSize imageSize = 4 * (VkDeviceSize)width * height;

	//Create staging buffer and copy image data to it
	Image texture = renderDevice->createImage2D(width, height, VK_FORMAT_R8G8B8A8_UNORM, 1, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	Buffer stagingBuffer = renderDevice->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	void* memory = nullptr;
	VK_CHECK(vkMapMemory(renderDevice->getDevice(), stagingBuffer.memory, 0, imageSize, 0, &memory));

	memcpy(memory, data.get(), imageSize);

	vkUnmapMemory(renderDevice->getDevice(), stagingBuffer.memory);

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

	VkSampler sampler;
	VK_CHECK(vkCreateSampler(renderDevice->getDevice(), &samplerCI, nullptr, &sampler));

	//Record buffer-to-image copy commands
	VkImageMemoryBarrier imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.srcAccessMask = 0;
	imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.image = texture.image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.layerCount = 1;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(commandBuffer,
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

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageBarrier.dstAccessMask = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

	stagingBuffers.push_back(stagingBuffer);

	return std::make_pair(texture, sampler);
}

void loadMaterials(const RaytracingDevice* device, const aiScene* scene, Scene& representation, const std::vector<uint32_t>& materialIndices)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	std::vector<Buffer> stagingBuffers;

	device->getRenderDevice()->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
		//Load materials
		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			int width;
			int height;
			std::shared_ptr<uint8_t> albedoData = nullptr;

			Material material;

			//Load albedo texture
			if (loadMaterialTexture(scene, scene->mMaterials[i], aiTextureType_DIFFUSE, 0, albedoData, width, height))
			{
				material.albedoIndex = (uint32_t)representation.textures.size();
				representation.textures.push_back(uploadTexture(device, commandBuffers[0], albedoData, width, height, stagingBuffers));
			}
			else
			{
				material.albedoIndex = (uint32_t)-1;
			}

			representation.materials.push_back(material);
		}

		//Write material data
		VkDeviceSize materialBufferSize = (VkDeviceSize)materialIndices.size() * sizeof(Material);

		representation.materialBuffer = renderDevice->createBuffer(materialBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		Buffer stagingBuffer = renderDevice->createBuffer(materialBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		Material* memory = nullptr;
		VK_CHECK(vkMapMemory(renderDevice->getDevice(), stagingBuffer.memory, 0, materialBufferSize, 0, (void**)&memory));

		for (size_t i = 0; i < materialIndices.size(); ++i)
		{
			const Material& material = representation.materials[materialIndices[i]];

			memory[i].albedoIndex = material.albedoIndex;
		}

		vkUnmapMemory(renderDevice->getDevice(), stagingBuffer.memory);

		VkBufferCopy region = { 0, 0, materialBufferSize };
		vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, representation.materialBuffer.buffer, 1, &region);

		stagingBuffers.push_back(stagingBuffer);
	});

	for (size_t i = 0; i < stagingBuffers.size(); ++i)
	{
		renderDevice->destroyBuffer(stagingBuffers[i]);
	}
}

void createSceneDescriptorSets(const RaytracingDevice* raytracingDevice, Scene& scene, const std::vector<uint32_t>& materialIndices)
{
	VkDevice device = raytracingDevice->getRenderDevice()->getDevice();

	//Create descriptor set layout
	VkDescriptorSetLayoutBinding layoutBinding[] = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)scene.meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)scene.meshBuffers.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)scene.materials.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr }
	};

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.pBindings = layoutBinding;
	layoutCI.bindingCount = sizeof(layoutBinding) / sizeof(layoutBinding[0]);
	
	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &scene.descriptorSetLayout));

	//Create descriptor pool
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * (uint32_t)scene.meshBuffers.size() + 1 },
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
	std::vector<VkWriteDescriptorSet> setWrites;

	//Write TLAS (binding = 0)
	VkAccelerationStructureKHR tlas = scene.tlas.get();

	VkWriteDescriptorSetAccelerationStructureKHR tlasSetWrite = {};
	tlasSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	tlasSetWrite.accelerationStructureCount = 1;
	tlasSetWrite.pAccelerationStructures = &tlas;

	setWrites.push_back({});
	setWrites.back().sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	setWrites.back().pNext = &tlasSetWrite;
	setWrites.back().dstSet = scene.descriptorSet;
	setWrites.back().dstBinding = 0;
	setWrites.back().descriptorCount = 1;
	setWrites.back().descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	//Write vertex buffers (binding = 1)
	std::vector<VkDescriptorBufferInfo> vertexSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), vertexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer.buffer, val.vertexOffset, val.vertexSize }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 1, vertexSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write index buffers (binding = 2)
	std::vector<VkDescriptorBufferInfo> indexSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), indexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.indexBuffer.buffer, val.indexOffset, val.indexSize }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 2, indexSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write textures (binding = 3)
	std::vector<VkDescriptorImageInfo> imageSetWrites(scene.textures.size());
	std::transform(scene.textures.begin(), scene.textures.end(), imageSetWrites.begin(), [](const std::pair<Image, VkSampler>& texture)
		{ return VkDescriptorImageInfo{ texture.second, texture.first.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }; });

	DESC_SET_WRITE_IMAGE(setWrites, scene.descriptorSet, 3, imageSetWrites, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	//Write material buffer (binding = 4)
	std::vector<VkDescriptorBufferInfo> materialSetWrites = { { scene.materialBuffer.buffer, 0, (VkDeviceSize)materialIndices.size() * sizeof(Material) } };

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 4, materialSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	vkUpdateDescriptorSets(device, (uint32_t)setWrites.size(), setWrites.data(), 0, nullptr);
}

std::shared_ptr<Scene> SceneLoader::loadScene(const RaytracingDevice* device, const char* scenePath)
{
	//Import scene from file
	std::cout << "Importing scene " << scenePath << "... ";

	auto start = std::chrono::high_resolution_clock::now();

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(scenePath, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs);

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;

	std::shared_ptr<Scene> representation = std::make_shared<Scene>();
	representation->device = device;

	std::vector<uint32_t> materialIndices;

	//Load scene graph (meshes)
	loadSceneGraph(device, scene, *representation, materialIndices);

	//Load materials
	loadMaterials(device, scene, *representation, materialIndices);

	//Create scene descriptor sets
	createSceneDescriptorSets(device, *representation, materialIndices);

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

	device->getRenderDevice()->destroyBuffer(materialBuffer);

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