#include "VulkanKHRRaytracer.h"

int main()
{
	VulkanKHRRaytracer* raytracer = VulkanKHRRaytracer::create();
	raytracer->start();

	delete raytracer;

	return 0;
}