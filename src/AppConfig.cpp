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

    AppConfig cfg;
    cfg.cms.listen_ip = read_required<std::string>(cms, "listen_ip", "cms");
    cfg.cms.multicast_group = read_required<std::string>(cms, "multicast_group", "cms");
    cfg.cms.multicast_port = read_port(cms, "multicast_port", "cms");

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

    if (root.contains("navs") && root.at("navs").is_object()) {
        const auto& navs = root.at("navs");
        cfg.navs.enabled = true;
        cfg.navs.listen_ip = read_required<std::string>(navs, "listen_ip", "navs");
        cfg.navs.multicast_group = read_required<std::string>(navs, "multicast_group", "navs");
        cfg.navs.multicast_port = read_port(navs, "multicast_port", "navs");

        if (navs.contains("topic_bindings") && navs.at("topic_bindings").is_array()) {
            for (const auto& binding : navs.at("topic_bindings")) {
                if (!binding.is_object()) {
                    throw std::runtime_error("Elemento non valido in 'navs.topic_bindings'");
                }

                const auto message_id = read_required<uint32_t>(binding, "message_id", "navs.topic_bindings");
                const auto topic = read_required<std::string>(binding, "topic", "navs.topic_bindings");

                NavsTopicBinding topic_binding;
                topic_binding.message_id = message_id;
                topic_binding.topic = topic;
                cfg.navs.topic_bindings.push_back(topic_binding);
            }
        }
    }

    return cfg;
}
