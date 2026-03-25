#include "AppConfig.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

template <typename T>
T read_required(const nlohmann::json& j, const char* key, const std::string& section_name) {
    if (!j.contains(key)) {
        throw std::runtime_error("Campo mancante in sezione '" + section_name + "': " + key);
    }
    return j.at(key).get<T>();
}

uint16_t read_port(const nlohmann::json& j, const char* key, const std::string& section_name) {
    const int value = read_required<int>(j, key, section_name);
    if (value < 1 || value > 65535) {
        throw std::runtime_error("Porta non valida in sezione '" + section_name + "': " + std::to_string(value));
    }
    return static_cast<uint16_t>(value);
}

} // namespace

AppConfig loadAppConfig(const std::string& config_path) {
    std::ifstream input(config_path);
    if (!input.is_open()) {
        throw std::runtime_error("Impossibile aprire il file di configurazione: " + config_path);
    }

    nlohmann::json root;
    input >> root;

    if (!root.contains("udp") || !root.at("udp").is_object()) {
        throw std::runtime_error("Sezione 'udp' mancante o non valida");
    }
    if (!root.contains("tcp") || !root.at("tcp").is_object()) {
        throw std::runtime_error("Sezione 'tcp' mancante o non valida");
    }
    const bool has_ack_section = root.contains("ack") && root.at("ack").is_object();
    const bool has_ack_multicast_section = root.contains("ack_multicast") && root.at("ack_multicast").is_object();
    if (!has_ack_section && !has_ack_multicast_section) {
        throw std::runtime_error("Sezione 'ack' o 'ack_multicast' mancante o non valida");
    }
    if (!root.contains("lrad_destinations") || !root.at("lrad_destinations").is_array()) {
        throw std::runtime_error("Sezione 'lrad_destinations' mancante o non valida");
    }

    const auto& udp = root.at("udp");
    const auto& tcp = root.at("tcp");
    const auto& ack = has_ack_section ? root.at("ack") : root.at("ack_multicast");
    const std::string ack_section_name = has_ack_section ? "ack" : "ack_multicast";

    AppConfig cfg;
    cfg.udp_listen_ip = read_required<std::string>(udp, "listen_ip", "udp");
    cfg.udp_multicast_group = read_required<std::string>(udp, "multicast_group", "udp");
    cfg.udp_multicast_port = read_port(udp, "multicast_port", "udp");

    cfg.tcp_default_target_ip = read_required<std::string>(tcp, "default_target_ip", "tcp");
    cfg.tcp_default_target_port = read_port(tcp, "default_target_port", "tcp");
    cfg.tcp_unicast_target_ip = read_required<std::string>(tcp, "unicast_target_ip", "tcp");

    cfg.ack_target_ip = read_required<std::string>(ack, "ip", ack_section_name);
    cfg.ack_target_port = read_port(ack, "port", ack_section_name);

    for (const auto& destination : root.at("lrad_destinations")) {
        if (!destination.is_object()) {
            throw std::runtime_error("Elemento non valido in 'lrad_destinations'");
        }

        const int id_value = read_required<int>(destination, "id", "lrad_destinations");
        if (id_value < 0 || id_value > 65535) {
            throw std::runtime_error("ID LRAD non valido: " + std::to_string(id_value));
        }

        const uint16_t id = static_cast<uint16_t>(id_value);
        LradDestination lrad;
        lrad.id = id;
        lrad.ip_address = read_required<std::string>(destination, "ip", "lrad_destinations");
        lrad.port = read_port(destination, "port", "lrad_destinations");

        cfg.lrad_destinations[id] = lrad;
    }

    if (cfg.lrad_destinations.empty()) {
        throw std::runtime_error("La lista 'lrad_destinations' e' vuota");
    }

    return cfg;
}
