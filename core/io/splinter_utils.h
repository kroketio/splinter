#include <filesystem>
#include <map>
#include <vector>

#include "core/string/ustring.h"
#include "core/typedefs.h"
#include "core/os/os.h"
#include "core/os/time.h"

namespace fs = std::filesystem;

namespace splinter {
  void ensureDirectories(const fs::path& path);
  bool fileExists(const std::string& path);
  bool fileExists(const fs::path& path);
  bool ensureDirectoryExists(std::string path);
  bool ensureDirectoryExists(fs::path path);

  struct PBRMat {
    std::string asset_pack;
    std::string name;
    std::string size;

    // e.g: /home/foo/Documents/godot_proj/asset_pack/brickwall56348.tres
    std::filesystem::path path_tres(const std::filesystem::path& project_path) const {
      std::filesystem::path y = project_path / "textures" / asset_pack / (name + ".tres");
      return y;
    }

    // e.g: /home/foo/Documents/godot_proj/asset_pack/
    fs::path baseDir(const std::filesystem::path& project_path) const {
      return path_tres(project_path).parent_path();
    }

    // e.g: make sure /home/foo/Documents/godot_proj/asset_pack/ exists
    void ensureBaseDirExists(const std::filesystem::path& project_path) const {
      auto base_dir = baseDir(project_path);
      ensureDirectories(base_dir);
    }

    bool exists_on_disk(const std::filesystem::path& project_path) {
      auto _path_tres = path_tres(project_path);
      return std::filesystem::exists(_path_tres);
    }
  };

  PBRMat parseGLTFMaterialKey(const String& gltf_material_key);
  void unpackAssetPackBuffer(const std::string& name, const std::vector<uint8_t>& buffer, const std::filesystem::path& basedir);
}