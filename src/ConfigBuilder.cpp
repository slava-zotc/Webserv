#include "ConfigBuilder.hpp"
#include <sstream>
#include <sys/stat.h>
#include <stdexcept>

static int parsePort(const std::string& raw) {
    std::stringstream ss(raw);
    int port;
    ss >> port;
    if (ss.fail() || !ss.eof() || port < 1 || port > 65535) {
        throw std::runtime_error("Invalid port in listen directive: " + raw);
    }
    return port;
}

static void validateRoot(const std::string& root) {
    struct stat st;
    if (stat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        throw std::runtime_error("Invalid root directive: " + root + " does not exist or is not a directory");
    }
}

static bool parseBool(const std::string& raw) {
    if (raw == "on" || raw == "true") return true;
    if (raw == "off" || raw == "false") return false;
    throw std::runtime_error("Invalid boolean value: " + raw);
}

static int parseAllowedMethods(const std::vector<std::string>& values) {
    int mask = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == "GET") mask |= Route::GET;
        else if (values[i] == "POST") mask |= Route::POST;
        else if (values[i] == "DELETE") mask |= Route::DELETE;
        else throw std::runtime_error("Invalid method in allowed_methods: " + values[i]);
    }
    return mask;
}

CFG_Route buildCfgRoute(const ConfigBlock& block, const std::string& parentRoot) {
    CFG_Route cfg = CFG_Route();
    cfg.prefix_ = block.arg;

    std::map<std::string, std::vector<std::string> >::const_iterator it;

    it = block.directives.find("root");
    if (it != block.directives.end() && !it->second.empty()) {
        cfg.root_ = it->second[0];
    } else {
        cfg.root_ = parentRoot;
    }
    if (cfg.root_.empty()) {
        throw std::runtime_error("No root defined for location " + cfg.prefix_ +
                                  " and no root inherited from server");
    }
    validateRoot(cfg.root_);

    it = block.directives.find("allowed_methods");
    if (it != block.directives.end()) {
        cfg.allowed_methods_ = parseAllowedMethods(it->second);
    }

    it = block.directives.find("autoindex");
    if (it != block.directives.end() && !it->second.empty()) {
        cfg.autoindex_ = parseBool(it->second[0]);
    }

    it = block.directives.find("index");
    if (it != block.directives.end() && !it->second.empty()) {
        cfg.index_file_ = it->second[0];
    }

    it = block.directives.find("upload_enabled");
    if (it != block.directives.end() && !it->second.empty()) {
        cfg.upload_enabled_ = parseBool(it->second[0]);
    }
    it = block.directives.find("upload_dir");
    if (it != block.directives.end() && !it->second.empty()) {
        cfg.upload_dir_ = it->second[0];
        if (cfg.upload_enabled_) {
            validateRoot(cfg.upload_dir_);
        }
    }

    it = block.directives.find("return");
    if (it != block.directives.end() && it->second.size() >= 2) {
        std::stringstream ss(it->second[0]);
        int code;
        ss >> code;
        if (ss.fail() || !ss.eof() || (code != 301 && code != 302)) {
            throw std::runtime_error("Invalid redirect code in return directive: " + it->second[0]);
        }
        cfg.has_redirect_ = true;
        cfg.redirect_target_ = it->second[1];
    }

    return cfg;
}

CFG_Server buildCfgServer(const ConfigBlock& block) {
    CFG_Server cfg = CFG_Server();

    const std::vector<std::string>& listenValues = block.directives.at("listen");
    if (listenValues.empty()) {
        throw std::runtime_error("Empty listen directive");
    }
    cfg.port_ = parsePort(listenValues[0]);

    const std::vector<std::string>& rootValues = block.directives.at("root");
    if (rootValues.empty()) {
        throw std::runtime_error("Empty root directive");
    }
    std::string root = rootValues[0];
    validateRoot(root);

    cfg.max_body_size_ = 1048576;

    std::map<std::string, std::vector<std::string> >::const_iterator it;

    it = block.directives.find("client_max_body_size");
    if (it != block.directives.end() && !it->second.empty()) {
        std::stringstream ss(it->second[0]);
        long size;
        ss >> size;
        if (ss.fail() || !ss.eof() || size < 0) {
            throw std::runtime_error("Invalid client_max_body_size: " + it->second[0]);
        }
        cfg.max_body_size_ = size;
    }

    it = block.directives.find("error_page");
    if (it != block.directives.end() && it->second.size() >= 2) {
        std::stringstream ss(it->second[0]);
        int code;
        ss >> code;
        if (ss.fail() || !ss.eof() || code < 100 || code > 599) {
            throw std::runtime_error("Invalid error_page code: " + it->second[0]);
        }
        cfg.error_pages_[code] = it->second[1];
    }

    for (size_t i = 0; i < block.children.size(); ++i) {
        if (block.children[i].name == "location") {
            cfg.routes_.push_back(buildCfgRoute(block.children[i], root));
        }
    }

    return cfg;
}
