#pragma once

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <cpr/cpr.h>
#include <string>
#include <utility>
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
    std::string base_cache_path;
    bool path_exists(const std::string& path) const;
    std::string get_image_path(const std::string& imageName) const;
    bool get_image(const std::string& imageName, std::string& outPath, std::string& error);
    bool pull_image_from_registry(const std::string& imageName, const std::string& imagePath, std::string& error);
    bool get_manifest(const std::string& imageName, const std::string& token, json& outManifest, std::string& error);
    bool get_auth_token(const std::string& imageName, std::string& outToken, std::string& error);
    std::string parse_auth_header(const std::string& header, const std::string& key);
    std::future<std::string> download_layer_async(const std::string& url, const std::string& token, const std::string& destination_path);
    bool extract_layer(const std::string& tarball_path, const std::string& destination_path, std::string& error);
    bool download_and_extract_layers(const json& manifest, const std::string& repo, const std::string& token, const std::string& destinationPath, std::string& error);

    // Download Bar Stuff
    std::map<std::string, DownloadProgress> m_download_progress;
    std::mutex m_progress_mutex;
    void print_progress();
};

struct DownloadProgress {
    long long total = 0;
    long long downloaded = 0;
};