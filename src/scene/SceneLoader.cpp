#include "SceneLoader.h"

#include <Common.h>

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>

#include <glm/gtx/transform.hpp>

#include <filesystem>
#include <vector>
#include <chrono>
#include <array>

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

//#define PROGRESS_GUARD(p) std::lock_guard<std::mutex> guard(p->lock)
//#define PROGRESS_TRANSACTION(p, x) if (p) { PROGRESS_GUARD(p); x; }

class ProgressTracker : public Assimp::ProgressHandler
{
private:
	std::shared_ptr<SceneLoadProgress> m_progress;
public:
	ProgressTracker(std::shared_ptr<SceneLoadProgress> progressVariable) :
		m_progress(progressVariable) {}

	bool Update(float percentage) override
	{
		m_progress->setStageProgress(percentage);

		return true;
	}
};

void parseSceneGraphNode(const RaytracingDevice* device, std::vector<VkAccelerationStructureInstanceKHR>& instances, const std::vector<BottomLevelAS>& blasList,
						 const aiScene* scene, aiNode* node, glm::mat4 transform, std::vector<uint32_t>& materialIndices, const std::vector<bool>& isMaterialOpaque)
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
		uint32_t materialIndex = scene->mMeshes[node->mMeshes[i]]->mMaterialIndex;
		
		//Compute geometry flags
		VkGeometryInstanceFlagsKHR flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		flags |= isMaterialOpaque[materialIndex] ? VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR : VK_GEOMETRY_INSTANCE_FORCE_NO_OPAQUE_BIT_KHR;

		//Add instance
		instances.push_back(device->compileInstances(blasList[node->mMeshes[i]], transform, node->mMeshes[i]/*gl_InstanceCustomIndexEXT*/, 0xFF, 0, flags));
		materialIndices.push_back(materialIndex);
	}

	//Traverse children
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		parseSceneGraphNode(device, instances, blasList, scene, node->mChildren[i], transform, materialIndices, isMaterialOpaque);
	}
}

template<int N>
struct BufferAllocDetails
{
	VkBuffer buffer;

	//The offset of the buffer within the allocation it takes its memory from
	VkDeviceSize pageOffset;

	//The actual size of the buffer (as returned by `vkGetBufferMemoryRequirements`)
	VkDeviceSize actualSize;

	//The total size of the ranges
	VkDeviceSize totalRangeSize;

	//The offset of each range WITHIN the buffer and its size
	std::array<std::pair<VkDeviceSize, VkDeviceSize>, N> ranges;
};

typedef BufferAllocDetails<3> VertexBufferAllocDetails;
typedef BufferAllocDetails<1> IndexBufferAllocDetails;

template<int N>
BufferAllocDetails<N> createBufferAllocDetails(VkDevice deviceHandle, VkDeviceSize sizes[N], VkDeviceSize rangeAlignment, VkBufferUsageFlags bufferUsage, VkDeviceSize& totalSceneSize, uint32_t& mutualMemoryTypeBits)
{
	BufferAllocDetails<N> allocDetails;

	for (int i = 0; i < N; ++i)
	{
		VkDeviceSize offset = i > 0 ? (offset = allocDetails.ranges[i - 1].first + allocDetails.ranges[i - 1].second) : 0;

		allocDetails.ranges[i] = std::make_pair(UINT32_ALIGN(offset, rangeAlignment), sizes[i]);
	}

	allocDetails.totalRangeSize = allocDetails.ranges.back().first + allocDetails.ranges.back().second;

	VkBufferCreateInfo bufferCI = {};
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.size = allocDetails.totalRangeSize;
	bufferCI.usage = bufferUsage;
	bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCI.queueFamilyIndexCount = 0;
	bufferCI.pQueueFamilyIndices = nullptr;

	VK_CHECK(vkCreateBuffer(deviceHandle, &bufferCI, nullptr, &allocDetails.buffer));

	//Compute offset and alignment, and update memory type bits
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(deviceHandle, allocDetails.buffer, &memRequirements);

	allocDetails.pageOffset = UINT32_ALIGN(totalSceneSize, memRequirements.alignment);
	allocDetails.actualSize = memRequirements.size;

	totalSceneSize = allocDetails.pageOffset + allocDetails.actualSize;

	//By ANDing the memory type bits of all the buffers together, we are
	//keeping only the memory types that are supported by all buffers
	mutualMemoryTypeBits &= memRequirements.memoryTypeBits;

	return allocDetails;
}

void loadSceneGraph(const RaytracingDevice* device, const aiScene* scene, Scene& representation, std::vector<uint32_t>& materialIndices)
{
	/*
	 ------------------------------
			Note on alignemt
	 ------------------------------

	 There are two alignemts that are of interest: 
		1. VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment
		2. VkMemoryRequirements::alignment

	 When allocating a buffer, its offset within the allocation it takes its memory from must be a multiple of
	 `VkMemoryRequirements::alignment`.

	 When binding a range of a buffer to a descriptor of type VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, its offset
	 within the buffer must be a multiple of `VkPhysicalDeviceLimits::minStorageBufferOffsetAlignment`.
	 This means that range offsets do not also need to be aligned to `VkMemoryRequirements::alignment`.
	*/

	//Parse meshes
	std::vector<BottomLevelAS> meshBLASList;

	VkDevice deviceHandle = device->getRenderDevice()->getDevice();

	const VkBufferUsageFlags vertexBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	const VkBufferUsageFlags indexBufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	const VkBufferUsageFlags stagingBufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	const VkMemoryPropertyFlags vertexMemoryProperty = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkMemoryPropertyFlags indexMemoryProperty = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	const VkMemoryPropertyFlags stagingMemoryProperty = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	//Calculate total allocation size
	VkDeviceSize totalSceneSize = 0;
	VkDeviceSize rangeAlingment = device->getPhysicalDeviceLimits().minStorageBufferOffsetAlignment;

	std::vector<VertexBufferAllocDetails> vertexBufferRanges;
	std::vector<IndexBufferAllocDetails> indexBufferRanges;

	uint32_t mutualMemoryTypeBits = 0xFFFFFFFF;

	//Calculate details of vertex buffers
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];

		VkDeviceSize sizes[3] = { mesh->mNumVertices * sizeof(glm::vec3),
								  mesh->mNumVertices * sizeof(glm::vec2),
								  mesh->mNumVertices * sizeof(glm::vec3) };

		vertexBufferRanges.push_back(createBufferAllocDetails<3>(deviceHandle, sizes, rangeAlingment, vertexBufferUsage, totalSceneSize, mutualMemoryTypeBits));
	}

	//Calculate details of index buffers
	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];

		VkDeviceSize sizes[1] = { mesh->mNumFaces * (3 * sizeof(uint32_t)) };

		indexBufferRanges.push_back(createBufferAllocDetails<1>(deviceHandle, sizes, rangeAlingment, vertexBufferUsage, totalSceneSize, mutualMemoryTypeBits));
	}

	if (!mutualMemoryTypeBits)
	{
		FATAL_ERROR("Could not find memory type that supports all scene buffers");
	}

	//Allocate scene memory
	uint32_t sceneMemTypeIndex = device->getRenderDevice()->findMemoryType(mutualMemoryTypeBits, vertexMemoryProperty);

	if (sceneMemTypeIndex == (uint32_t)-1)
	{
		FATAL_ERROR("Could not find appropriate memory type for scene");
	}

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = totalSceneSize;
	memAllocInfo.memoryTypeIndex = sceneMemTypeIndex;

	VkDeviceMemory sceneMemory = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateMemory(deviceHandle, &memAllocInfo, nullptr, &sceneMemory));
	
	//Allocate staging memory
	Buffer stagingBuffer = device->getRenderDevice()->createBuffer(totalSceneSize, stagingBufferUsage, stagingMemoryProperty);

	device->getRenderDevice()->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
		for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
		{
			const aiMesh* mesh = scene->mMeshes[i];

			const VertexBufferAllocDetails& vertexBufferDetails = vertexBufferRanges[i];
			const IndexBufferAllocDetails& indexBufferDetails = indexBufferRanges[i];

			//Bind buffer memory
			vkBindBufferMemory(deviceHandle, vertexBufferDetails.buffer, sceneMemory, vertexBufferDetails.pageOffset);
			vkBindBufferMemory(deviceHandle, indexBufferDetails.buffer, sceneMemory, indexBufferDetails.pageOffset);

			//Write vertex data
			void* memory = nullptr;
			VK_CHECK(vkMapMemory(deviceHandle, stagingBuffer.memory, vertexBufferDetails.pageOffset, vertexBufferDetails.totalRangeSize, 0, &memory));

			glm::vec3* positionMemory = (glm::vec3*)((uint8_t*)memory + vertexBufferDetails.ranges[0].first);
			glm::vec2* texCoordMemory = (glm::vec2*)((uint8_t*)memory + vertexBufferDetails.ranges[1].first);
			glm::vec3* normalMemory = (glm::vec3*)((uint8_t*)memory + vertexBufferDetails.ranges[2].first);

			for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D pos = mesh->mVertices[i];
				positionMemory[i] = { pos.x, pos.y, pos.z };
				
				aiVector3D normal = mesh->mNormals[i];
				normalMemory[i] = { normal.x, normal.y, normal.z };

				aiVector3D texCoords = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
				texCoordMemory[i] = { texCoords.x, texCoords.y };
			}

			vkUnmapMemory(deviceHandle, stagingBuffer.memory);

			//Write index data
			VK_CHECK(vkMapMemory(deviceHandle, stagingBuffer.memory, indexBufferDetails.pageOffset, indexBufferDetails.totalRangeSize, 0, &memory));

			unsigned int* indexMemory = (unsigned int*)((uint8_t*)memory + indexBufferDetails.ranges[0].first);
			for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
			{
				assert(mesh->mFaces[i].mNumIndices == 3);

				indexMemory[3 * i] = mesh->mFaces[i].mIndices[0];
				indexMemory[3 * i + 1] = mesh->mFaces[i].mIndices[1];
				indexMemory[3 * i + 2] = mesh->mFaces[i].mIndices[2];
			}

			vkUnmapMemory(deviceHandle, stagingBuffer.memory);

			//Create BLAS for mesh
			std::shared_ptr<const BLASGeometryInfo> geomertryInfo = device->compileGeometry(vertexBufferDetails.buffer, sizeof(glm::vec3), mesh->mNumVertices, indexBufferDetails.buffer, mesh->mNumFaces, { 0 }, 0);

			BottomLevelAS blas;
			blas.init(device, geomertryInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

			meshBLASList.push_back(blas);

			representation.meshBuffers.push_back({
				vertexBufferDetails.buffer,
				vertexBufferDetails.ranges[0],
				vertexBufferDetails.ranges[1],
				vertexBufferDetails.ranges[2],

				indexBufferDetails.buffer,
				indexBufferDetails.ranges[0].first,
				indexBufferDetails.ranges[0].second
			});

			//Record copy commands
			VkBufferCopy copyRegion = { vertexBufferDetails.pageOffset, 0, vertexBufferDetails.totalRangeSize };
			vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, vertexBufferDetails.buffer, 1, &copyRegion);

			copyRegion = { indexBufferDetails.pageOffset, 0, indexBufferDetails.totalRangeSize };
			vkCmdCopyBuffer(commandBuffers[0], stagingBuffer.buffer, indexBufferDetails.buffer, 1, &copyRegion);
		}
	});

	//Free staging buffers
	representation.sceneMemory = sceneMemory;

	device->getRenderDevice()->destroyBuffer(stagingBuffer);

	device->buildBLAS(meshBLASList);

	//Parse scene graph
	std::vector<VkAccelerationStructureInstanceKHR> accelStructInstances;

	parseSceneGraphNode(device, accelStructInstances, meshBLASList, scene, scene->mRootNode, glm::identity<glm::mat4>(), materialIndices, representation.isMaterialOpaque);

	//Build TLAS
	TopLevelAS tlas;
	tlas.init(device);

	device->buildTLAS(tlas, accelStructInstances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	representation.blasList = std::move(meshBLASList);
	representation.tlas = std::move(tlas);
}

bool loadMaterialTexture(const aiScene* scene, const char* scenePath, const aiMaterial* material, aiTextureType textureType, int index, std::shared_ptr<uint8_t>& output, int& width, int& height)
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
		std::filesystem::path path(Resources::resolvePath(name.C_Str()));

		if (path.is_relative())
		{
			path = std::filesystem::path(scenePath).replace_filename(path);
		}

		std::string pathString = path.string();

		int numChannels;
		stbi_uc* imageMemory = stbi_load(pathString.c_str(), &width, &height, &numChannels, 4);

		if (!imageMemory)
		{
			std::cerr << "Unable to load texture '" << name.C_Str() << "' from material '" << material->GetName().C_Str() << "'" << std::endl;
			return false;
		}

		output = std::shared_ptr<uint8_t>((uint8_t*)imageMemory, stbi_image_free);
	}

	return true;
}

bool checkHasAlpha(std::shared_ptr<uint8_t> output, int width, int height)
{
	uint32_t* pixelPointer = (uint32_t*)output.get();

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			if (((pixelPointer[y * width + x] >> 24) & 0xFF) != 0xFF)
			{
				return true;
			}
		}
	}

	return false;
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

void loadMaterials(const RaytracingDevice* device, const char* scenePath, const aiScene* scene, Scene& representation, std::shared_ptr<SceneLoadProgress> progress)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	std::vector<Buffer> stagingBuffers;

	std::cout << "Loading materials (" << scene->mNumMaterials << ")... ";

	auto start = std::chrono::high_resolution_clock::now();

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
			if (loadMaterialTexture(scene, scenePath, scene->mMaterials[i], aiTextureType_DIFFUSE, 0, albedoData, width, height))
			{
				representation.isMaterialOpaque.push_back(!checkHasAlpha(albedoData, width, height));

				material.albedoIndex = (uint32_t)representation.textures.size();
				representation.textures.push_back(uploadTexture(device, commandBuffers[0], albedoData, width, height, stagingBuffers));
			}
			else
			{
				representation.isMaterialOpaque.push_back(true);

				material.albedoIndex = (uint32_t)-1;
			}

			progress->setStageProgress((float)i / (scene->mNumMaterials + 1));

			representation.materials.push_back(material);
		}
	});

	progress->setStageProgress(1.0f);

	for (size_t i = 0; i < stagingBuffers.size(); ++i)
	{
		renderDevice->destroyBuffer(stagingBuffers[i]);
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;
}

void uploadMaterialMappings(const RaytracingDevice* device, Scene& representation, const std::vector<uint32_t>& materialIndices)
{
	const RenderDevice* renderDevice = device->getRenderDevice();

	std::vector<Buffer> stagingBuffers;

	device->getRenderDevice()->executeCommands(1, [&](VkCommandBuffer* commandBuffers)
	{
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
	uint32_t meshBufferCount = (uint32_t)scene.meshBuffers.size();

	VkDescriptorSetLayoutBinding layoutBinding[] = {
		{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferCount, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferCount, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferCount, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, meshBufferCount, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)scene.textures.size(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
		{ 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr }
	};

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.pBindings = layoutBinding;
	layoutCI.bindingCount = sizeof(layoutBinding) / sizeof(layoutBinding[0]);
	
	VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &scene.descriptorSetLayout));

	//Create descriptor pool
	VkDescriptorPoolSize descPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * meshBufferCount + 1 },
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
	setWrites.back().descriptorCount = 1 ;
	setWrites.back().descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	//Write position buffers (binding = 1)
	std::vector<VkDescriptorBufferInfo> vertexPosSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), vertexPosSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer, val.positionRange.first, val.positionRange.second }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 1, vertexPosSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write normal buffers (binding = 2)
	std::vector<VkDescriptorBufferInfo> vertexNormalSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), vertexNormalSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer, val.normalRange.first, val.normalRange.second }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 2, vertexNormalSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write texCoord buffers (binding = 3)
	std::vector<VkDescriptorBufferInfo> vertexTexCoordSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), vertexTexCoordSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.vertexBuffer, val.texCoordRange.first, val.texCoordRange.second }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 3, vertexTexCoordSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write index buffers (binding = 4)
	std::vector<VkDescriptorBufferInfo> indexSetWrites(scene.meshBuffers.size());
	std::transform(scene.meshBuffers.begin(), scene.meshBuffers.end(), indexSetWrites.begin(), [](const MeshBuffers& val)
		{ return VkDescriptorBufferInfo{ val.indexBuffer, val.indexOffset, val.indexSize }; });

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 4, indexSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	//Write textures (binding = 5)
	std::vector<VkDescriptorImageInfo> imageSetWrites(scene.textures.size());
	std::transform(scene.textures.begin(), scene.textures.end(), imageSetWrites.begin(), [](const std::pair<Image, VkSampler>& texture)
		{ return VkDescriptorImageInfo{ texture.second, texture.first.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }; });

	DESC_SET_WRITE_IMAGE(setWrites, scene.descriptorSet, 5, imageSetWrites, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	//Write material buffer (binding = 6)
	std::vector<VkDescriptorBufferInfo> materialSetWrites = { { scene.materialBuffer.buffer, 0, (VkDeviceSize)materialIndices.size() * sizeof(Material) } };

	DESC_SET_WRITE_BUFFER(setWrites, scene.descriptorSet, 6, materialSetWrites, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	vkUpdateDescriptorSets(device, (uint32_t)setWrites.size(), setWrites.data(), 0, nullptr);
}

std::shared_ptr<Scene> SceneLoader::loadScene(const RaytracingDevice* device, const char* scenePath, std::shared_ptr<SceneLoadProgress> progress)
{
	if (!progress)
	{
		progress = std::make_shared<SceneLoadProgress>();
	}

	progress->begin(4, "Importing scene");

	//Import scene from file
	std::cout << "Importing scene " << scenePath << "... ";

	std::string path = Resources::resolvePath(scenePath);

	auto start = std::chrono::high_resolution_clock::now();

	Assimp::Importer importer;
	importer.SetProgressHandler(new ProgressTracker(progress));

	const aiScene* scene = importer.ReadFile(path.c_str(), aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;

	if (!scene)
	{
		std::cout << "Assimp Error:\n" << importer.GetErrorString() << std::endl;

		progress->finish();

		return nullptr;
	}

	progress->nextStage("Loading materials");

	std::shared_ptr<Scene> representation = std::make_shared<Scene>();
	representation->device = device;

	std::vector<uint32_t> materialIndices;

	//Load materials
	loadMaterials(device, scenePath, scene, *representation, progress);

	progress->nextStage("Loading scene graph");

	//Load scene graph (meshes)
	loadSceneGraph(device, scene, *representation, materialIndices);

	//Upload material mapping indices
	uploadMaterialMappings(device, *representation, materialIndices);

	progress->nextStage("Creating descriptors");

	//Create scene descriptor sets
	createSceneDescriptorSets(device, *representation, materialIndices);

	//Load camera data
	if (scene->HasCameras())
	{
		const aiCamera* camera = scene->mCameras[0];

		aiNode* cameraNode = scene->mRootNode->FindNode(camera->mName.data);

		aiMatrix4x4 T;
		while (cameraNode != scene->mRootNode)
		{
			T = cameraNode->mTransformation * T;
			cameraNode = cameraNode->mParent;
		}

		aiMatrix4x4 R = T;
		R.a4 = R.b4 = R.c4 = 0.f;
		R.d4 = 1.f;

		// Transform
		aiVector3D from = T * camera->mPosition;
		aiVector3D forward = R * camera->mLookAt;
		aiVector3D up = R * camera->mUp;

		representation->cameraPosition = glm::vec3(from.x, from.y, from.z);
		representation->cameraRotation = glm::quatLookAt(glm::vec3(forward.x, forward.y, forward.z), glm::vec3(up.x, up.y, up.z));
	}
	else
	{
		representation->cameraPosition = glm::vec3(0, 0, 0);
		representation->cameraRotation = glm::identity<glm::quat>();
	}

	importer.FreeScene();

	progress->finish();
	
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
		vkDestroyBuffer(deviceHandle, buffers.vertexBuffer, nullptr);
		vkDestroyBuffer(deviceHandle, buffers.indexBuffer, nullptr);
	}

	vkFreeMemory(deviceHandle, sceneMemory, nullptr);

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