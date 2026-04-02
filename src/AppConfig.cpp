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

    if (!root.contains("cms") || !root.at("cms").is_object()) {
        throw std::runtime_error("Sezione 'cms' mancante o non valida");
    }
    const auto& cms = root.at("cms");
    if (!cms.contains("handlers") || !cms.at("handlers").is_object()) {
        throw std::runtime_error("Sezione 'cms.handlers' mancante o non valida");
    }
    const auto& handlers = cms.at("handlers");
    if (!handlers.contains("tcp_send") || !handlers.at("tcp_send").is_object()) {
        throw std::runtime_error("Sezione 'cms.handlers.tcp_send' mancante o non valida");
    }
    if (!handlers.contains("ack_send") || !handlers.at("ack_send").is_object()) {
        throw std::runtime_error("Sezione 'cms.handlers.ack_send' mancante o non valida");
    }
    const auto& tcp_send = handlers.at("tcp_send");
    if (!tcp_send.contains("lrad_destinations") || !tcp_send.at("lrad_destinations").is_array()) {
        throw std::runtime_error("Sezione 'cms.handlers.tcp_send.lrad_destinations' mancante o non valida");
    }
    const auto& ack_send = handlers.at("ack_send");

    AppConfig cfg;
    cfg.cms.listen_ip = read_required<std::string>(cms, "listen_ip", "cms");
    cfg.cms.multicast_group = read_required<std::string>(cms, "multicast_group", "cms");
    cfg.cms.multicast_port = read_port(cms, "multicast_port", "cms");

    cfg.cms.handlers.ack_send.target_ip = read_required<std::string>(ack_send, "target_ip", "cms.handlers.ack_send");
    cfg.cms.handlers.ack_send.target_port = read_port(ack_send, "target_port", "cms.handlers.ack_send");

    if (handlers.contains("udp_unicast_send") && handlers.at("udp_unicast_send").is_object()) {
        const auto& udp_unicast_send = handlers.at("udp_unicast_send");
        if (udp_unicast_send.contains("enabled")) {
            cfg.cms.handlers.udp_unicast_send.enabled = udp_unicast_send.at("enabled").get<bool>();
        }

        if (cfg.cms.handlers.udp_unicast_send.enabled) {
            cfg.cms.handlers.udp_unicast_send.target_ip = read_required<std::string>(
                udp_unicast_send,
                "target_ip",
                "cms.handlers.udp_unicast_send"
            );
            cfg.cms.handlers.udp_unicast_send.target_port = read_port(
                udp_unicast_send,
                "target_port",
                "cms.handlers.udp_unicast_send"
            );
        }
    }

    if (cms.contains("periodic_health_status") && cms.at("periodic_health_status").is_object()) {
        const auto& periodic_health_status = cms.at("periodic_health_status");
        if (periodic_health_status.contains("enabled")) {
            cfg.cms.periodic_health_status.enabled = periodic_health_status.at("enabled").get<bool>();
        }
        if (periodic_health_status.contains("interval_ms")) {
            const int interval_ms = periodic_health_status.at("interval_ms").get<int>();
            if (interval_ms < 1) {
                throw std::runtime_error("Campo non valido in sezione 'cms.periodic_health_status': interval_ms");
            }
            cfg.cms.periodic_health_status.interval_ms = static_cast<uint32_t>(interval_ms);
        }
    }

    for (const auto& destination : tcp_send.at("lrad_destinations")) {
        if (!destination.is_object()) {
            throw std::runtime_error("Elemento non valido in 'cms.handlers.tcp_send.lrad_destinations'");
        }

        const int id_value = read_required<int>(destination, "id", "cms.handlers.tcp_send.lrad_destinations");
        if (id_value < 0 || id_value > 65535) {
            throw std::runtime_error("ID LRAD non valido: " + std::to_string(id_value));
        }

        const uint16_t id = static_cast<uint16_t>(id_value);
        LradDestination lrad;
        lrad.id = id;
        lrad.ip_address = read_required<std::string>(destination, "ip", "cms.handlers.tcp_send.lrad_destinations");
        lrad.port = read_port(destination, "port", "cms.handlers.tcp_send.lrad_destinations");

        cfg.cms.handlers.tcp_send.lrad_destinations[id] = lrad;
    }

    if (cfg.cms.handlers.tcp_send.lrad_destinations.empty()) {
        throw std::runtime_error("La lista 'cms.handlers.tcp_send.lrad_destinations' e' vuota");
    }

    // Parsing unicast_relays (opzionale)
    if (cms.contains("unicast_relays") && cms.at("unicast_relays").is_array()) {
        for (const auto& relay : cms.at("unicast_relays")) {
            if (!relay.is_object()) {
                throw std::runtime_error("Elemento non valido in 'cms.unicast_relays'");
            }

            CmsUnicastRelayConfig relay_cfg;
            relay_cfg.name = read_required<std::string>(relay, "name", "cms.unicast_relays");
            relay_cfg.destination_ip = read_required<std::string>(relay, "destination_ip", "cms.unicast_relays");
            relay_cfg.destination_port = read_port(relay, "destination_port", "cms.unicast_relays");

            cfg.cms.unicast_relays.push_back(relay_cfg);
        }
    }

    cfg.acs.listen_ip = "127.0.0.1";
    cfg.acs.listen_port = 56100;

    if (root.contains("acs") && root.at("acs").is_object()) {
        const auto& acs = root.at("acs");
        cfg.acs.listen_ip = read_required<std::string>(acs, "listen_ip", "acs");
        cfg.acs.listen_port = read_port(acs, "listen_port", "acs");

        if (acs.contains("destinations") && acs.at("destinations").is_array()) {
            for (const auto& destination : acs.at("destinations")) {
                if (!destination.is_object()) {
                    throw std::runtime_error("Elemento non valido in 'acs.destinations'");
                }

                const int id_value = read_required<int>(destination, "id", "acs.destinations");
                if (id_value < 0 || id_value > 65535) {
                    throw std::runtime_error("ID ACS non valido: " + std::to_string(id_value));
                }

                const uint16_t id = static_cast<uint16_t>(id_value);
                AcsDestination acs_destination;
                acs_destination.id = id;
                acs_destination.ip_address = read_required<std::string>(destination, "ip", "acs.destinations");
                acs_destination.port = read_port(destination, "port", "acs.destinations");
                cfg.acs.destinations[id] = acs_destination;
            }
        }
    }

    return cfg;
}
