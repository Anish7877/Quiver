#pragma once

#include <string>
#include <vector>
#include <future>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class ImageManager {
public:
    
    ImageManager();
    bool pull(const std::string& imageName, std::string& outPath, std::string& error);
    bool remove(const std::string& imageName, std::string& error);
    static void handle_error(const std::string& message);

private:
    std::string baseCachePath;
    bool path_exists(const std::string& path) const;
    std::string get_image_path(const std::string& imageName) const;
    bool getImage(const std::string& imageName, std::string& outPath, std::string& error);
    bool pullImageFromRegistry(const std::string& imageName, const std::string& imagePath, std::string& error);
    bool getManifest(const std::string& imageName, const std::string& token, json& outManifest, std::string& error);
    bool getAuthToken(const std::string& imageName, std::string& outToken, std::string& error);
    std::string parseAuthHeader(const std::string& header, const std::string& key);
    std::future<std::string> download_layer_async(const std::string& url, const std::string& token, const std::string& destination_path);
    bool extract_layer(const std::string& tarball_path, const std::string& destination_path, std::string& error);
    bool downloadAndExtractLayers(const json& manifest, const std::string& repo, const std::string& token, const std::string& destinationPath, std::string& error);
};