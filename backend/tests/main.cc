#include <gtest/gtest.h>
#include <drogon/drogon.h>
#include <json/json.h>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

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
    auto dbUrl = std::getenv("DATABASE_URL");
    if (!dbUrl) {
        dbUrl = std::getenv("DB_URL");
    }
    auto redisHost = std::getenv("REDIS_HOST");
    auto redisPort = std::getenv("REDIS_PORT");
    auto redisPass = std::getenv("REDIS_PASSWORD");
    auto redisUrl = std::getenv("REDIS_URL");
    auto jwtAccessSecret = std::getenv("JWT_ACCESS_SECRET");
    auto jwtRefreshSecret = std::getenv("JWT_REFRESH_SECRET");
    auto appPepper = std::getenv("APP_PEPPER");
    auto aesEncryptionKey = std::getenv("AES_ENCRYPTION_KEY");
    
    if (dbUrl) {
        std::string urlStr(dbUrl);
        std::string prefix1 = "postgresql://";
        std::string prefix2 = "postgres://";
        std::string remaining = urlStr;
        
        if (urlStr.find(prefix1) == 0) {
            remaining = urlStr.substr(prefix1.length());
        } else if (urlStr.find(prefix2) == 0) {
            remaining = urlStr.substr(prefix2.length());
        }
        
        size_t atPos = remaining.find('@');
        std::string user = "";
        std::string pass = "";
        if (atPos != std::string::npos) {
            std::string userPass = remaining.substr(0, atPos);
            remaining = remaining.substr(atPos + 1);
            
            size_t colonPos = userPass.find(':');
            if (colonPos != std::string::npos) {
                user = userPass.substr(0, colonPos);
                pass = userPass.substr(colonPos + 1);
            } else {
                user = userPass;
            }
        }
        
        size_t slashPos = remaining.find('/');
        std::string hostPort = remaining;
        std::string dbNameStr = "";
        if (slashPos != std::string::npos) {
            hostPort = remaining.substr(0, slashPos);
            dbNameStr = remaining.substr(slashPos + 1);
        }
        
        size_t questionPos = dbNameStr.find('?');
        if (questionPos != std::string::npos) {
            dbNameStr = dbNameStr.substr(0, questionPos);
        }
        
        size_t colonPos = hostPort.find(':');
        std::string host = hostPort;
        std::string port = "5432";
        if (colonPos != std::string::npos) {
            host = hostPort.substr(0, colonPos);
            port = hostPort.substr(colonPos + 1);
        }
        
        config["db_clients"][0]["host"] = host;
        config["db_clients"][0]["port"] = std::atoi(port.c_str());
        config["db_clients"][0]["user"] = user;
        config["db_clients"][0]["passwd"] = pass;
        config["db_clients"][0]["dbname"] = dbNameStr;
        
        std::cout << "Tests config override from DATABASE_URL: host=" << host 
                  << ", port=" << port 
                  << ", user=" << user 
                  << ", dbname=" << dbNameStr 
                  << ", password=[REDACTED]" << std::endl;
    } else {
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
    }
    if (redisUrl) {
        std::string urlStr(redisUrl);
        std::string schemaPrefix = "redis://";
        std::string secureSchemaPrefix = "rediss://";
        std::string remaining = urlStr;
        
        if (urlStr.find(schemaPrefix) == 0) {
            remaining = urlStr.substr(schemaPrefix.length());
        } else if (urlStr.find(secureSchemaPrefix) == 0) {
            remaining = urlStr.substr(secureSchemaPrefix.length());
        }
        
        size_t atPos = remaining.find('@');
        std::string pass = "";
        if (atPos != std::string::npos) {
            std::string userPass = remaining.substr(0, atPos);
            remaining = remaining.substr(atPos + 1);
            size_t colonPos = userPass.find(':');
            if (colonPos != std::string::npos) {
                pass = userPass.substr(colonPos + 1);
            } else {
                pass = userPass;
            }
        }
        
        size_t colonPos = remaining.find(':');
        std::string host = remaining;
        std::string port = "6379";
        if (colonPos != std::string::npos) {
            host = remaining.substr(0, colonPos);
            port = remaining.substr(colonPos + 1);
        }
        
        config["redis_clients"][0]["host"] = host;
        config["redis_clients"][0]["port"] = std::atoi(port.c_str());
        config["redis_clients"][0]["passwd"] = pass;
        std::cout << "Tests config override from REDIS_URL: host=" << host << ", port=" << port << ", password=" << (pass.empty() ? "None" : "[REDACTED]") << std::endl;
    } else {
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
    }
    if (jwtAccessSecret) {
        config["custom_config"]["jwt_access_secret"] = jwtAccessSecret;
        std::cout << "Tests config override: JWT_ACCESS_SECRET=[REDACTED]" << std::endl;
    }
    if (jwtRefreshSecret) {
        config["custom_config"]["jwt_refresh_secret"] = jwtRefreshSecret;
        std::cout << "Tests config override: JWT_REFRESH_SECRET=[REDACTED]" << std::endl;
    }
    if (appPepper) {
        config["custom_config"]["app_pepper"] = appPepper;
        std::cout << "Tests config override: APP_PEPPER=[REDACTED]" << std::endl;
    }
    if (aesEncryptionKey) {
        config["custom_config"]["aes_encryption_key"] = aesEncryptionKey;
        std::cout << "Tests config override: AES_ENCRYPTION_KEY=[REDACTED]" << std::endl;
    }
    
    std::ofstream outfile(outputPath);
    Json::FastWriter writer;
    outfile << writer.write(config);
}

int main(int argc, char **argv) {
    trantor::Logger::setLogLevel(trantor::Logger::kDebug);
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
    
    std::thread thr([](){
        drogon::app().run();
    });
    
    // Wait for DB client to initialize
    int attempts = 0;
    while (!drogon::app().getDbClient() && attempts < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }

    auto db = drogon::app().getDbClient();
    if (db) {
        try {
            db->execSqlSync("ALTER TABLE loans ADD COLUMN IF NOT EXISTS name VARCHAR(255) NOT NULL DEFAULT 'Personal Loan'");
            db->execSqlSync("ALTER TABLE loans ADD COLUMN IF NOT EXISTS reference_number VARCHAR(100) UNIQUE");
            auto res = db->execSqlSync("SELECT id FROM loans WHERE reference_number IS NULL");
            for (auto const& row : res) {
                std::string id = row["id"].as<std::string>();
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(100000, 999999);
                std::string ref = "LN-" + std::to_string(dis(gen));
                db->execSqlSync("UPDATE loans SET reference_number = $1 WHERE id = $2", ref, id);
            }
            db->execSqlSync("ALTER TABLE loans ALTER COLUMN reference_number SET NOT NULL");
            db->execSqlSync("CREATE UNIQUE INDEX IF NOT EXISTS idx_loans_reference ON loans(reference_number)");
        } catch (const std::exception& e) {
            std::cerr << "Failed to run DB migrations in tests: " << e.what() << std::endl;
        }
    }
    
    int ret = RUN_ALL_TESTS();
    
    drogon::app().quit();
    if (thr.joinable()) {
        thr.join();
    }
    return ret;
}
