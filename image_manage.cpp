#include "imagemanager.hpp" 
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <sys/stat.h>
#include <cpr/cpr.h>


ImageManager::ImageManager() {
    const char* homeDir = std::getenv("HOME");
    if (homeDir != nullptr) {
        this->baseCachePath = std::string(homeDir) + "/.quiver/images";
    }
}

bool ImageManager::pull(const std::string& imageName, std::string& outPath, std::string& error) {
    if (baseCachePath.empty()) {
        error = "CRITICAL: HOME environment variable is not set.";
        return false;
    }
    return getImage(imageName, outPath, error);
}

bool ImageManager::remove(const std::string& imageName, std::string& error) {
    if (baseCachePath.empty()) {
        error = "CRITICAL: HOME environment variable is not set.";
        return false;
    }
    std::string imagePath = get_image_path(imageName);
    if (!path_exists(imagePath)) {
        error = "Image not found locally: " + imageName;
        return false;
    }

    std::cout << "Removing image at " << imagePath << std::endl;
    std::string rm_command = "rm -rf " + imagePath;
    if (system(rm_command.c_str()) != 0) {
        error = "Error removing image.";
        return false;
    }
    std::cout << "Image removed successfully." << std::endl;
    return true;
}

void ImageManager::handle_error(const std::string& message) {
    std::cerr << "\nERROR: " << message << std::endl;
}

bool ImageManager::path_exists(const std::string& path) const {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

std::string ImageManager::get_image_path(const std::string& imageName) const {
    std::string safeImageName = imageName;
    std::replace(safeImageName.begin(), safeImageName.end(), ':', '_');
    std::replace(safeImageName.begin(), safeImageName.end(), '/', '_');
    return this->baseCachePath + "/" + safeImageName;
}

bool ImageManager::getImage(const std::string& imageName, std::string& outPath, std::string& error) {
    outPath = get_image_path(imageName);
    if (path_exists(outPath)) {
        std::cout << "Image found in local cache." << std::endl;
        return true;
    }

    std::cout << "Image not in cache. Pulling from registry..." << std::endl;
    if (!pullImageFromRegistry(imageName, outPath, error)) {
        if (path_exists(outPath)) {
            std::string rm_command = "rm -rf " + outPath;
            system(rm_command.c_str());
        }
        error = "Failed to pull image. Reason: " + error;
        return false;
    }
    
    std::cout << "Pull complete." << std::endl;
    return true;
}

bool ImageManager::pullImageFromRegistry(const std::string& imageName, const std::string& imagePath, std::string& error) {
    std::string repo, tag;
    size_t colon_pos = imageName.find(':');
    repo = (colon_pos != std::string::npos) ? imageName.substr(0, colon_pos) : imageName;
    tag = (colon_pos != std::string::npos) ? imageName.substr(colon_pos + 1) : "latest";
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    std::cout << "Authenticating..." << std::endl;
    std::string token;
    if (!getAuthToken(imageName, token, error)) return false;

    std::cout << "Fetching manifest for '" << tag << "'..." << std::endl;
    json manifest;
    if (!getManifest(imageName, token, manifest, error)) return false;

    if (manifest.contains("manifests")) {
        std::cout << "Detected a manifest list. Searching for 'amd64' architecture." << std::endl;
        std::string arch_digest;
        for (const auto& entry : manifest["manifests"]) {
            if (entry.contains("platform") && entry["platform"]["architecture"] == "amd64") {
                arch_digest = entry["digest"];
                break;
            }
        }
        if (arch_digest.empty()) {
            error = "Could not find an 'amd64' manifest.";
            return false;
        }
        
        std::cout << "Found 'amd64' digest. Fetching architecture-specific manifest..." << std::endl;
        std::string arch_image_name = repo + "@" + arch_digest;
        if (!getManifest(arch_image_name, token, manifest, error)) return false;
    }

    std::cout << "Downloading layers..." << std::endl;
    return downloadAndExtractLayers(manifest, repo, token, imagePath, error);
}

bool ImageManager::getManifest(const std::string& imageName, const std::string& token, json& outManifest, std::string& error) {
    std::string repo, reference;
    size_t at_pos = imageName.find('@');
    if (at_pos != std::string::npos) {
        repo = imageName.substr(0, at_pos);
        reference = imageName.substr(at_pos + 1);
    } else {
        size_t colon_pos = imageName.find(':');
        repo = (colon_pos != std::string::npos) ? imageName.substr(0, colon_pos) : imageName;
        reference = (colon_pos != std::string::npos) ? imageName.substr(colon_pos + 1) : "latest";
    }
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    cpr::Url manifestUrl{"https://registry-1.docker.io/v2/" + repo + "/manifests/" + reference};
    cpr::Response r = cpr::Get(manifestUrl, cpr::Header{
        {"Authorization", "Bearer " + token},
        {"Accept", "application/vnd.docker.distribution.manifest.v2+json, application/vnd.oci.image.index.v1+json"}
    });
    if (r.status_code != 200) {
        error = "Failed to get manifest: " + std::to_string(r.status_code) + " " + r.reason;
        return false;
    }
    outManifest = json::parse(r.text);
    return true;
}

bool ImageManager::getAuthToken(const std::string& imageName, std::string& outToken, std::string& error) {
    std::string repo;
    size_t colon_pos = imageName.find(':');
    repo = (colon_pos != std::string::npos) ? imageName.substr(0, colon_pos) : imageName;
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    cpr::Response r = cpr::Get(cpr::Url{"https://registry-1.docker.io/v2/"});
    if (r.header.find("Www-Authenticate") == r.header.end()) {
        error = "Could not find Www-Authenticate header.";
        return false;
    }
    
    std::string authHeader = r.header["Www-Authenticate"];
    std::string realm = parseAuthHeader(authHeader, "realm");
    std::string service = parseAuthHeader(authHeader, "service");
    if (realm.empty() || service.empty()) {
        error = "Failed to parse realm or service from auth header.";
        return false;
    }
    
    cpr::Url authUrl{realm};
    cpr::Parameters params{{"service", service}, {"scope", "repository:" + repo + ":pull"}};
    cpr::Response tokenResponse = cpr::Get(authUrl, params);
    if (tokenResponse.status_code != 200) {
        error = "Failed to get auth token: " + std::to_string(tokenResponse.status_code) + " - " + tokenResponse.text;
        return false;
    }
    
    json tokenJson = json::parse(tokenResponse.text);
    if (!tokenJson.contains("token")) {
        error = "Auth token response did not contain a 'token' field.";
        return false;
    }
    outToken = tokenJson["token"];
    return true;
}

std::string ImageManager::parseAuthHeader(const std::string& header, const std::string& key) {
    size_t key_pos = header.find(key + "=\"");
    if (key_pos == std::string::npos) return "";
    size_t value_start = key_pos + key.length() + 2;
    size_t value_end = header.find("\"", value_start);
    if (value_end == std::string::npos) return "";
    return header.substr(value_start, value_end - value_start);
}

std::future<std::string> ImageManager::download_layer_async(const std::string& url, const std::string& token, const std::string& destination_path) {
    return std::async(std::launch::async, [url, token, destination_path]() -> std::string {
        std::ofstream outFile(destination_path, std::ios::binary);
        if (!outFile) return "Failed to open file for writing: " + destination_path;
        
        cpr::Response r = cpr::Download(outFile, cpr::Url{url}, cpr::Header{{"Authorization", "Bearer " + token}});
        outFile.close();

        if (r.status_code != 200 && r.status_code != 307) {
            return "Failed to download layer to " + destination_path + ". Status: " + std::to_string(r.status_code);
        }
        return "";
    });
}

bool ImageManager::extract_layer(const std::string& tarball_path, const std::string& destination_path, std::string& error) {
    std::string command = "tar -xzf " + tarball_path + " -C " + destination_path;
    if (system(command.c_str()) != 0) {
        error = "Failed to extract layer " + tarball_path;
        return false;
    }
    return true;
}

bool ImageManager::downloadAndExtractLayers(const json& manifest, const std::string& repo, const std::string& token, const std::string& destinationPath, std::string& error) {
    system(("mkdir -p " + destinationPath).c_str());

    if (!manifest.contains("layers") || manifest["layers"].empty()) {
        error = "Manifest does not contain any 'layers'.";
        return false;
    }
    
    std::vector<std::string> digests;
    for (const auto& layer : manifest["layers"]) digests.push_back(layer["digest"]);

    std::vector<std::future<std::string>> download_futures;
    std::vector<std::string> tarball_paths;

    std::cout << "Launching " << digests.size() << " layer downloads in parallel..." << std::endl;
    for (const auto& digest : digests) {
        std::string url = "https://registry-1.docker.io/v2/" + repo + "/blobs/" + digest;
        std::string path = destinationPath + "/" + digest.substr(7) + ".tar.gz";
        tarball_paths.push_back(path);
        download_futures.push_back(download_layer_async(url, token, path));
    }

    std::cout << "Waiting for downloads to complete and extracting layers sequentially..." << std::endl;
    for (size_t i = 0; i < digests.size(); ++i) {
        std::cout << "Waiting for layer " << i + 1 << "/" << digests.size() << " (" << digests[i].substr(0, 20) << ")..." << std::endl;
        
        std::string download_error = download_futures[i].get();
        if (!download_error.empty()) {
            error = download_error;
            return false;
        }
        
        std::cout << "  Download complete. Extracting layer " << i + 1 << "..." << std::endl;
        if (!extract_layer(tarball_paths[i], destinationPath, error)) {
            return false;
        }
        std::remove(tarball_paths[i].c_str());
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <pull|rm> <image_name>" << std::endl;
        std::cerr << "Example: " << argv[0] << " pull ubuntu:latest" << std::endl;
        return 1;
    }

    ImageManager manager;
    std::string command = argv[1];
    std::string imageName = argv[2];
    std::string error_message;

    if (command == "pull") {
        std::string finalPath;
        if (manager.pull(imageName, finalPath, error_message)) {
            std::cout << "\nSuccess! Image is available at:" << std::endl;
            std::cout << finalPath << std::endl;
        } else {
            ImageManager::handle_error(error_message);
            return 1;
        }
    } else if (command == "rm") {
        if (!manager.remove(imageName, error_message)) {
            ImageManager::handle_error(error_message);
            return 1;
        }
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}