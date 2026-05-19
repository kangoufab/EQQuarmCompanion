#include "core/config.h"
#include <fstream>
#include <stdexcept>

Config::Config(std::filesystem::path configPath,
               std::filesystem::path defaultsPath)
    : _configPath(std::move(configPath))
{
    std::ifstream df(defaultsPath);
    if (!df.is_open())
        throw std::runtime_error("config_defaults.json introuvable : "
                                 + defaultsPath.string());
    _data = nlohmann::json::parse(df);

    if (std::filesystem::exists(_configPath)) {
        try {
            std::ifstream cf(_configPath);
            auto user = nlohmann::json::parse(cf);
            _data.merge_patch(user);
        } catch (const std::exception&) {
            // config.json illisible → defaults utilisés
        }
    }
}

std::string Config::get(std::string_view key) const {
    auto it = _data.find(key);
    if (it == _data.end()) return {};
    return it->is_string() ? it->get<std::string>() : it->dump();
}

nlohmann::json Config::getJson(std::string_view key) const {
    auto it = _data.find(key);
    return (it != _data.end()) ? *it : nlohmann::json{};
}

void Config::set(std::string_view key, nlohmann::json value) {
    _data[std::string(key)] = std::move(value);
}

void Config::save() const {
    std::ofstream f(_configPath);
    if (!f.is_open())
        throw std::runtime_error("Impossible d'écrire config.json : "
                                 + _configPath.string());
    f << _data.dump(2);
}

DbConfig Config::getDbConfig() const {
    DbConfig d;
    if (auto it = _data.find("db"); it != _data.end()) {
        d.host     = it->value("host",     "localhost");
        d.port     = it->value("port",     3306);
        d.user     = it->value("user",     "root");
        d.password = it->value("password", "");
        d.database = it->value("database", "quarm");
    }
    return d;
}

std::map<std::string, float> Config::getClassWeights(
    std::string_view className) const
{
    std::map<std::string, float> weights;
    auto cw = _data.find("class_weights");
    if (cw == _data.end()) return weights;
    auto cls = cw->find(std::string(className));
    if (cls == cw->end()) return weights;
    for (auto& [k, v] : cls->items()) {
        if (v.is_number())
            weights[k] = v.get<float>();
    }
    return weights;
}

void Config::setClassWeights(std::string_view className,
                              const std::map<std::string, float>& w) {
    auto& cw = _data["class_weights"][std::string(className)];
    for (auto& [k, v] : w) cw[k] = v;
}

std::string Config::getCharacterClass(std::string_view charName) const {
    try {
        return _data.at("characters").at(std::string(charName))
                    .at("class").get<std::string>();
    } catch (const std::exception&) { return {}; }
}

void Config::setCharacterClass(std::string_view charName,
                                std::string_view className) {
    _data["characters"][std::string(charName)]["class"] = std::string(className);
}

nlohmann::json Config::getCharacterJson(std::string_view charName,
                                         std::string_view key) const {
    try {
        return _data.at("characters").at(std::string(charName)).at(std::string(key));
    } catch (const std::exception&) { return nlohmann::json{}; }
}

void Config::setCharacterJson(std::string_view charName, std::string_view key,
                               nlohmann::json value) {
    _data["characters"][std::string(charName)][std::string(key)] = std::move(value);
}
