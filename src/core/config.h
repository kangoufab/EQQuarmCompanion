#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

struct DbConfig {
    std::string host, user, password, database;
    int port{3306};
};

class Config {
public:
    Config(std::filesystem::path configPath,
           std::filesystem::path defaultsPath);

    [[nodiscard]] std::string get(std::string_view key) const;
    void set(std::string_view key, nlohmann::json value);
    void save() const;

    [[nodiscard]] DbConfig getDbConfig() const;
    [[nodiscard]] std::map<std::string, float> getClassWeights(
        std::string_view className) const;
    void setClassWeights(std::string_view className,
                         const std::map<std::string, float>&);
    [[nodiscard]] std::string getCharacterClass(std::string_view charName) const;
    void setCharacterClass(std::string_view charName, std::string_view className);

private:
    std::filesystem::path _configPath;
    nlohmann::json        _data;
};
