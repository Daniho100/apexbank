#include <gtest/gtest.h>
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <cstdlib>
#include <iostream>

void resolveConfig(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream infile(inputPath);
    if (!infile.is_open()) {
        throw std::runtime_error("Could not open input config: " + inputPath);
    }
    
    Json::Value config;
    Json::Reader reader;
    if (!reader.parse(infile, config)) {
        throw std::runtime_error("Failed to parse config file: " + inputPath);
    }
    
    auto dbHost = std::getenv("DB_HOST");
    auto dbPort = std::getenv("DB_PORT");
    auto dbUser = std::getenv("DB_USER");
    auto dbPass = std::getenv("DB_PASSWORD");
    auto dbName = std::getenv("DB_NAME");
    auto redisHost = std::getenv("REDIS_HOST");
    auto redisPort = std::getenv("REDIS_PORT");
    auto redisPass = std::getenv("REDIS_PASSWORD");
    
    if (dbHost) {
        config["db_clients"][0]["host"] = dbHost;
        std::cout << "Tests config override: DB_HOST=" << dbHost << std::endl;
    }
    if (dbPort) {
        config["db_clients"][0]["port"] = std::atoi(dbPort);
        std::cout << "Tests config override: DB_PORT=" << dbPort << std::endl;
    }
    if (dbUser) {
        config["db_clients"][0]["user"] = dbUser;
        std::cout << "Tests config override: DB_USER=" << dbUser << std::endl;
    }
    if (dbPass) {
        config["db_clients"][0]["passwd"] = dbPass;
        std::cout << "Tests config override: DB_PASSWORD=[REDACTED]" << std::endl;
    }
    if (dbName) {
        config["db_clients"][0]["dbname"] = dbName;
        std::cout << "Tests config override: DB_NAME=" << dbName << std::endl;
    }
    if (redisHost) {
        config["redis_clients"][0]["host"] = redisHost;
        std::cout << "Tests config override: REDIS_HOST=" << redisHost << std::endl;
    }
    if (redisPort) {
        config["redis_clients"][0]["port"] = std::atoi(redisPort);
        std::cout << "Tests config override: REDIS_PORT=" << redisPort << std::endl;
    }
    if (redisPass) {
        config["redis_clients"][0]["passwd"] = redisPass;
        std::cout << "Tests config override: REDIS_PASSWORD=[REDACTED]" << std::endl;
    }
    
    std::ofstream outfile(outputPath);
    Json::FastWriter writer;
    outfile << writer.write(config);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    
    // Resolve dynamic configurations for container isolation
    std::string originalConfig = "../config.json";
    std::string resolvedConfig = "config_resolved.json";
    
    try {
        resolveConfig(originalConfig, resolvedConfig);
        drogon::app().loadConfigFile(resolvedConfig);
    } catch (const std::exception& e) {
        std::cerr << "Config resolution failed: " << e.what() << std::endl;
        std::cerr << "Attempting to load original file directly..." << std::endl;
        try {
            drogon::app().loadConfigFile(originalConfig);
        } catch(...) {
            try {
                drogon::app().loadConfigFile("config.json");
            } catch(...) {
                std::cerr << "Warning: No configuration loaded. Tests might fail." << std::endl;
            }
        }
    }
    
    return RUN_ALL_TESTS();
}
