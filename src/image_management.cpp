#include "../include/image_management.hpp"

ImageManager::ImageManager() {
    const char* home_dir { getenv("HOME") };
    if (home_dir != nullptr) {
        this->base_cache_path = std::string(home_dir) + "/.quiver/images";
    }
}

bool ImageManager::pull(const std::string& image_name, std::string& out_path, std::string& error) {
    if (base_cache_path.empty()) {
        error = "CRITICAL: HOME environment variable is not set.";
        return false;
    }
    return get_image(image_name, out_path, error);
}

bool ImageManager::remove(const std::string& image_name, std::string& error) {
    if (base_cache_path.empty()) {
        error = "CRITICAL: HOME environment variable is not set.";
        return false;
    }
    std::string image_path { get_image_path(image_name) };
    if (!path_exists(image_path)) {
        error = "Image not found locally: " + image_name;
        return false;
    }

    std::cout << "Removing image at " << image_path << '\n';
    std::string rm_command { "rm -rf " + image_path };
    if (system(rm_command.c_str()) != 0) {
        error = "Error removing image.";
        return false;
    }
    std::cout << "Image removed successfully." << '\n';
    return true;
}

void ImageManager::handle_error(const std::string& message) {
    std::cerr << "\nERROR: " << message << '\n';
}

bool ImageManager::path_exists(const std::string& path) const {
    struct stat buffer{};
    return (stat(path.c_str(), &buffer) == 0);
}

std::string ImageManager::get_image_path(const std::string& image_name) const {
    std::string safe_image_name { image_name };
    std::replace(safe_image_name.begin(), safe_image_name.end(), ':', '_');
    std::replace(safe_image_name.begin(), safe_image_name.end(), '/', '_');
    return this->base_cache_path + "/" + safe_image_name;
}

bool ImageManager::get_image(const std::string& image_name, std::string& out_path, std::string& error) {
    out_path = get_image_path(image_name);
    if (path_exists(out_path)) {
        std::cout << "Image found in local storage." << '\n';
        return true;
    }

    std::cout << "Image not in local storage. Pulling from registry..." << '\n';
    if (!pull_image_from_registry(image_name, out_path, error)) {
        if (path_exists(out_path)) {
            std::string rm_command { "rm -rf " + out_path };
            system(rm_command.c_str());
        }
        error = "Failed to pull image. Reason: " + error;
        return false;
    }

    std::cout << "Pull complete." << '\n';
    return true;
}

bool ImageManager::pull_image_from_registry(const std::string& image_name, const std::string& image_path, std::string& error) {
    std::string repo{}, tag{};
    size_t colon_pos { image_name.find(':') };
    repo = (colon_pos != std::string::npos) ? image_name.substr(0, colon_pos) : image_name;
    tag = (colon_pos != std::string::npos) ? image_name.substr(colon_pos + 1) : "latest";
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    std::cout << "Authenticating..." << '\n';
    std::string token{};
    if (!get_auth_token(image_name, token, error)) return false;

    std::cout << "Fetching manifest for '" << tag << "'..." << '\n';
    json manifest{};
    if (!get_manifest(image_name, token, manifest, error)) return false;

    if (manifest.contains("manifests")) {
        std::cout << "Detected a manifest list. Searching for 'amd64' architecture." << '\n';
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

        std::cout << "Found 'amd64' digest. Fetching architecture-specific manifest..." << '\n';
        std::string arch_image_name = repo + "@" + arch_digest;
        if (!get_manifest(arch_image_name, token, manifest, error)) return false;
    }

    std::cout << "Downloading layers..." << '\n';
    return download_and_extract_layers(manifest, repo, token, image_path, error);
}

bool ImageManager::get_manifest(const std::string& image_name, const std::string& token, json& out_manifest, std::string& error) {
    std::string repo, reference;
    size_t at_pos = image_name.find('@');
    if (at_pos != std::string::npos) {
        repo = image_name.substr(0, at_pos);
        reference = image_name.substr(at_pos + 1);
    } else {
        size_t colon_pos = image_name.find(':');
        repo = (colon_pos != std::string::npos) ? image_name.substr(0, colon_pos) : image_name;
        reference = (colon_pos != std::string::npos) ? image_name.substr(colon_pos + 1) : "latest";
    }
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    cpr::Url manifest_url{"https://registry-1.docker.io/v2/" + repo + "/manifests/" + reference};
    cpr::Response r = cpr::Get(manifest_url, cpr::Header{
        {"Authorization", "Bearer " + token},
        {"Accept", "application/vnd.docker.distribution.manifest.v2+json, application/vnd.oci.image.index.v1+json"}
    });
    if (r.status_code != 200) {
        error = "Failed to get manifest: " + std::to_string(r.status_code) + " " + r.reason;
        return false;
    }
    out_manifest = json::parse(r.text);
    return true;
}

bool ImageManager::get_auth_token(const std::string& image_name, std::string& out_token, std::string& error) {
    std::string repo;
    size_t colon_pos = image_name.find(':');
    repo = (colon_pos != std::string::npos) ? image_name.substr(0, colon_pos) : image_name;
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;

    cpr::Response r = cpr::Get(cpr::Url{"https://registry-1.docker.io/v2/"});
    if (r.header.find("Www-Authenticate") == r.header.end()) {
        error = "Could not find Www-Authenticate header.";
        return false;
    }

    std::string auth_header = r.header["Www-Authenticate"];
    std::string realm = parse_auth_header(auth_header, "realm");
    std::string service = parse_auth_header(auth_header, "service");
    if (realm.empty() || service.empty()) {
        error = "Failed to parse realm or service from auth header.";
        return false;
    }

    cpr::Url auth_url{realm};
    cpr::Parameters params{{"service", service}, {"scope", "repository:" + repo + ":pull"}};
    cpr::Response token_response = cpr::Get(auth_url, params);
    if (token_response.status_code != 200) {
        error = "Failed to get auth token: " + std::to_string(token_response.status_code) + " - " + token_response.text;
        return false;
    }

    json token_json = json::parse(token_response.text);
    if (!token_json.contains("token")) {
        error = "Auth token response did not contain a 'token' field.";
        return false;
    }
    out_token = token_json["token"];
    return true;
}

std::string ImageManager::parse_auth_header(const std::string& header, const std::string& key) {
    size_t key_pos = header.find(key + "=\"");
    if (key_pos == std::string::npos) return "";
    size_t value_start = key_pos + key.length() + 2;
    size_t value_end = header.find("\"", value_start);
    if (value_end == std::string::npos) return "";
    return header.substr(value_start, value_end - value_start);
}

std::future<std::string> ImageManager::download_layer_async(const std::string& url, const std::string& token, const std::string& destination_path) {
    return std::async(std::launch::async, [url, token, destination_path]() -> std::string {
        std::ofstream out_file(destination_path, std::ios::binary);
        if (!out_file) return "Failed to open file for writing: " + destination_path;

        cpr::Response r = cpr::Download(out_file, cpr::Url{url}, cpr::Header{{"Authorization", "Bearer " + token}});
        out_file.close();

        if (r.status_code != 200 && r.status_code != 307) {
            return "Failed to download layer to " + destination_path + ". Status: " + std::to_string(r.status_code);
        }
        return "";
    });
}

bool ImageManager::extract_layer(const std::string& tarball_path, const std::string& destination_path, std::string& error) {
    std::string command { "tar -xzf " + tarball_path + " -C " + destination_path };
    if (system(command.c_str()) != 0) {
        error = "Failed to extract layer " + tarball_path;
        return false;
    }
    return true;
}

bool ImageManager::download_and_extract_layers(const json& manifest, const std::string& repo, const std::string& token, const std::string& destination_path, std::string& error) {
    system(("mkdir -p " + destination_path).c_str());

    if (!manifest.contains("layers") || manifest["layers"].empty()) {
        error = "Manifest does not contain any 'layers'.";
        return false;
    }

    std::vector<std::string> digests{};
    for (const auto& layer : manifest["layers"]) digests.push_back(layer["digest"]);

    std::vector<std::future<std::string>> download_futures{};
    std::vector<std::string> tarball_paths{};

    std::cout << "Launching " << digests.size() << " layer downloads in parallel..." << '\n';
    for (const auto& digest : digests) {
        std::string url = "https://registry-1.docker.io/v2/" + repo + "/blobs/" + digest;
        std::string path = destination_path + "/" + digest.substr(7) + ".tar.gz";
        tarball_paths.push_back(path);
        download_futures.push_back(download_layer_async(url, token, path));
    }

    std::cout << "Waiting for downloads to complete and extracting layers sequentially..." << '\n';
    for (size_t i = 0; i < digests.size(); ++i) {
        std::cout << "Waiting for layer " << i + 1 << "/" << digests.size() << " (" << digests[i].substr(0, 20) << ")..." << '\n';

        std::string download_error = download_futures[i].get();
        if (!download_error.empty()) {
            error = download_error;
            return false;
        }

        std::cout << "  Download complete. Extracting layer " << i + 1 << "..." << '\n';
        if (!extract_layer(tarball_paths[i], destination_path, error)) {
            return false;
        }
        std::remove(tarball_paths[i].c_str());
    }
    return true;
}
