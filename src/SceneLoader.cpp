#include "SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <vector>
#include <chrono>

SceneRepresentation SceneLoader::loadScene(const RaytracingDevice* device, const char* scenePath)
{
	//Import scene from file
	std::cout << "Importing scene " << scenePath << "... ";

	auto start = std::chrono::high_resolution_clock::now();

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(scenePath, aiProcessPreset_TargetRealtime_MaxQuality);

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;

	//Parse scene
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
			}

			unsigned int* indexMemory = (unsigned int*)&vertexMemory[mesh->mNumVertices];
			for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
			{
				assert(mesh->mFaces[i].mNumIndices == 3);

				indexMemory[3 * i] = mesh->mFaces[i].mIndices[0];
				indexMemory[3 * i + 1] = mesh->mFaces[i].mIndices[1];
				indexMemory[3 * i + 3] = mesh->mFaces[i].mIndices[2];
			}

			vkUnmapMemory(device->getRenderDevice()->getDevice(), stagingBuffer.memory);

			//Create BLAS for mesh
			Buffer vertexBuffer = device->getRenderDevice()->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			Buffer indexBuffer = device->getRenderDevice()->createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																						  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			std::shared_ptr<const BLASGeometryInfo> geomertryInfo = device->compileGeometry(vertexBuffer, sizeof(MeshVertex), mesh->mNumVertices, indexBuffer, { 0 });

			meshBLASList[i].init(device, geomertryInfo, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);

			stagingBuffers[i] = stagingBuffer;

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
		
	importer.FreeScene();

	SceneRepresentation representation;
	representation.device = device;
	representation.blasList = std::move(meshBLASList);

	return representation;
};

void SceneRepresentation::destroy()
{
	for (BottomLevelAS& blas : blasList)
	{
		blas.destroy();

		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->indexBuffer);
		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->vertexBuffer);
	}
}