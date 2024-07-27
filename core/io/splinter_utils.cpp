#include "splinter_utils.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>

#include "core/variant/variant.h"
#include "core/io/json.h"

namespace splinter {
	namespace fs = std::filesystem;

	PBRMat parseGLTFMaterialKey(const String& gltf_material_key) {
		Vector<String> parts = gltf_material_key.split("/");

		if (parts.size() >= 3) {
			String asset_pack = parts[1];
			String texture = parts[2];

			Vector<String> texture_parts = texture.split("_");
			if (texture_parts.size() == 3) {
				String texture_name = texture_parts[0];
				String texture_size = texture_parts[1];
				String texture_type = texture_parts[2];

				if (texture_type.ends_with("color")) {
					auto _asset_pack = std::string(asset_pack.utf8().get_data());
					auto _name = std::string(texture_name.utf8().get_data());
					return PBRMat{
						.asset_pack = _asset_pack,
						.name = _name,
						.size = std::string(texture_size.utf8().get_data())
					};
				}
			}
		}

		return {};
	}

	bool fileExists(const std::string& path) {
		return std::filesystem::exists(path);
	}

	bool fileExists(const fs::path& path) {
		return std::filesystem::exists(path);
	}

	bool ensureDirectoryExists(std::string path) {
		fs::path _path = path;
		return ensureDirectoryExists(_path);
	}

	void ensureDirectories(const fs::path& path) {
		fs::create_directories(path);
	}

	bool ensureDirectoryExists(fs::path path) {
		if (!fs::exists(path)) {
			print_line(vformat("creating directory: %s", path.string().c_str()));
			fs::create_directories(path);
			return true;
		}
		return false;
	}

	void unpackAssetPackBuffer(const std::string& name, const std::vector<uint8_t>& buffer, const std::filesystem::path& basedir) {
	    if (!fs::exists(basedir)) {
	    	print_error(vformat("asset_server; Base directory for material item name %s", name.c_str()));
	        return;
	    }

		if (buffer.size() == 0 || buffer[0] != '[') {
			print_error(vformat("asset_server; bad buffer, size == 0 or does not start with [ for material item name %s", name.c_str()));
			return;
		}

		auto it = std::find(buffer.begin(), buffer.end(), '\n');
		if (it == buffer.end()) {
			print_error(vformat("asset_server; No JSON header found for material item name %s", name.c_str()));
			return;
		}

		std::string header;
		std::vector<uint8_t> fileDataSection;

		if (it != buffer.end()) {
			header = std::string(buffer.begin(), it);
			fileDataSection = std::vector<uint8_t>(it + 1, buffer.end());
		}

		JSON json;
		String jsonString(header.c_str());
		Error err = json.parse(jsonString);
		if (err != OK) {
			print_error(vformat("asset_server: json_parse; err for material item name %s", name.c_str()));
			return;
		}

		Array jsonArray = json.get_data();
	    for (int i = 0; i < jsonArray.size(); i++) {
	        Dictionary obj = jsonArray[i];
	        String fileName = obj["fileName"];
	        int64_t offset = obj["offset"];
	        int64_t size = obj["fileSize"];

	        std::vector<uint8_t> fileData(fileDataSection.begin() + offset, fileDataSection.begin() + offset + size);
	        std::string filename_str = fileName.utf8().get_data();
	        std::filesystem::path filePath = basedir / filename_str;

	        fs::create_directories(filePath.parent_path());

	        std::ofstream outFile(filePath, std::ios::binary);
	        if (outFile.is_open()) {
	            outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
	            outFile.close();
	        } else {
	        	print_error(vformat("asset_server: Failed to write file for material item name %s", name.c_str()));
	        }
	    }
	}
}