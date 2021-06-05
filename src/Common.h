#pragma once

#include <fstream>
#include <cassert>

#include "ProjectBase.h"

class Resources
{
public:
	inline static std::string resolvePath(const char* path)
	{
		static const char* resourcePrefix = "asset://";
		static const size_t prefixLength = strlen(resourcePrefix);

		static const std::string dataDirectory = std::string(DATA_DIRECTORY_PATH) + std::string("/");

		if (!strncmp(path, resourcePrefix, prefixLength))
		{
			return dataDirectory + std::string(path + prefixLength);
		}

		return std::string(path);
	}

	inline static std::string loadShader(const char* path)
	{
		std::string fullPath = resolvePath(path);

		std::ifstream in(fullPath, std::ios::in | std::ios::binary);
		if (!in)
		{
			return "";
		}

		std::string contents;

		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);

		in.read(contents.data(), contents.size());
		in.close();

		return contents;
	}

	inline static std::string shaderIncludeDir()
	{
		return std::string(DATA_DIRECTORY_PATH) + "/shaders/";
	}
};

#define FATAL_ERROR(...) printf(__VA_ARGS__); assert(false)