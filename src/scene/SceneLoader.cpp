#include "SceneLoader.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include <glm/gtx/transform.hpp>

#include <vector>
#include <chrono>

void parseSceneGraphNode(const RaytracingDevice* device, std::vector<VkAccelerationStructureInstanceKHR>& instances, const std::vector<BottomLevelAS>& blasList, aiNode* node, glm::mat4 transform)
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
		instances.push_back(device->compileInstances(blasList[node->mMeshes[i]], transform, 0/*gl_InstanceCustomIndexEXT*/, 0xFF, 0, VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR));
	}

	//Traverse children
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		parseSceneGraphNode(device, instances, blasList, node->mChildren[i], transform);
	}
}

void loadSceneGraph(const RaytracingDevice* device, const aiScene* scene, SceneRepresentation& representation)
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
			Buffer vertexBuffer = device->getRenderDevice()->createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																							VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			Buffer indexBuffer = device->getRenderDevice()->createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
																						VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			std::shared_ptr<const BLASGeometryInfo> geomertryInfo = device->compileGeometry(vertexBuffer, sizeof(MeshVertex), mesh->mNumVertices, indexBuffer, mesh->mNumFaces, { 0 });

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

	//Parse scene graph
	std::vector<VkAccelerationStructureInstanceKHR> accelStructInstances;
	parseSceneGraphNode(device, accelStructInstances, meshBLASList, scene->mRootNode, glm::identity<glm::mat4>());

	//Build TLAS
	TopLevelAS tlas;
	tlas.init(device);

	device->buildTLAS(tlas, accelStructInstances, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

	representation.blasList = std::move(meshBLASList);
	representation.tlas = std::move(tlas);
}

SceneRepresentation SceneLoader::loadScene(const RaytracingDevice* device, const char* scenePath)
{
	//Import scene from file
	std::cout << "Importing scene " << scenePath << "... ";

	auto start = std::chrono::high_resolution_clock::now();

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(scenePath, aiProcessPreset_TargetRealtime_MaxQuality);

	auto end = std::chrono::high_resolution_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f << "s" << std::endl;

	SceneRepresentation representation;
	representation.device = device;

	//Load scene graph (meshes)
	loadSceneGraph(device, scene, representation);

	importer.FreeScene();
	
	return representation;
};

void SceneRepresentation::destroy()
{
	tlas.destroy();

	for (BottomLevelAS& blas : blasList)
	{
		blas.destroy();

		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->indexBuffer);
		device->getRenderDevice()->destroyBuffer(blas.getGeometryInfo()->vertexBuffer);
	}
}