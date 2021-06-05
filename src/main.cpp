#include <iostream>
#include <thread>
#include <chrono>

#include "ProjectBase.h"

#include "VulkanKHRRaytracer.h"

#include "scene/ScenePresenter.h"

#include "scene/pipelines/SamplerZooPipeline.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>

int main()
{
	VulkanKHRRaytracer* raytracer = VulkanKHRRaytracer::create();
	raytracer->start();

	delete raytracer;

	return 0;
}