#pragma once

#include <fstream>

#include "ProjectBase.h"

class Resources
{
public:
	inline static std::string resolvePath(const char* path, bool absolute = false)
	{
		return absolute ? std::string(path) : (std::string(DATA_DIRECTORY_PATH) + "/" + std::string(path));
	}

	inline static std::string loadShader(const char* path, bool absolute = false)
	{
		std::string fullPath = resolvePath(path, absolute);

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