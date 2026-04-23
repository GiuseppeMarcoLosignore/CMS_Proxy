#include "AppConfig.hpp"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <stdexcept>

namespace {

namespace pt = boost::property_tree;

uint16_t get_port(const pt::ptree& tree, const std::string& path) {
    const int value = tree.get<int>(path);
    if (value < 1 || value > 65535) {
        throw std::runtime_error("Porta non valida: " + std::to_string(value) + " (" + path + ")");
    }
    return static_cast<uint16_t>(value);
}

CmsConfig parse_cms(const pt::ptree& root) {
    if (!root.get_child_optional("cms")) {
        throw std::runtime_error("Sezione 'cms' mancante o non valida");
    }

    CmsConfig cfg;
    cfg.multicast_group = root.get<std::string>("cms.multicast_group");
    cfg.multicast_port  = get_port(root, "cms.multicast_port");

    if (const auto relays = root.get_child_optional("cms.unicast_relay")) {
        for (const auto& [key, relay] : *relays) {
            CmsUnicastRelayConfig r;
            r.name             = relay.get<std::string>("name");
            r.destination_ip   = relay.get<std::string>("destination_ip");
            r.destination_port = get_port(relay, "destination_port");
            cfg.unicast_relays.push_back(r);
        }
    }

    return cfg;
}

AcsConfig parse_acs(const pt::ptree& root) {
    AcsConfig cfg;
    cfg.multicast_group    = "226.1.1.30";
    cfg.multicast_port     = 56100;
    cfg.tcp_listen_ip      = "127.0.0.1";
    cfg.tcp_listen_port    = 56101;
    cfg.tx_multicast_group = cfg.multicast_group;
    cfg.tx_multicast_port  = cfg.multicast_port;
    cfg.destinations[1]    = { 1, "127.0.0.1", 9000 };
    cfg.destinations[2]    = { 2, "127.0.0.1", 9000 };

    if (!root.get_child_optional("acs")) {
        return cfg;
    }

    cfg.multicast_group    = root.get<std::string>("acs.multicast_group");
    cfg.multicast_port     = get_port(root, "acs.multicast_port");
    cfg.tx_multicast_group = cfg.multicast_group;
    cfg.tx_multicast_port  = cfg.multicast_port;

    if (root.get_child_optional("acs.tcp_unicast")) {
        cfg.tcp_listen_ip   = root.get<std::string>("acs.tcp_unicast.listen_ip");
        cfg.tcp_listen_port = get_port(root, "acs.tcp_unicast.listen_port");
    }

    if (root.get_child_optional("acs.multicast_tx")) {
        cfg.tx_multicast_group = root.get<std::string>("acs.multicast_tx.group");
        cfg.tx_multicast_port  = get_port(root, "acs.multicast_tx.port");
    }

    if (const auto dests = root.get_child_optional("acs.destination")) {
        for (const auto& [key, dest_node] : *dests) {
            const int id_value = dest_node.get<int>("id");
            if (id_value < 0 || id_value > 65535) {
                throw std::runtime_error("ID ACS non valido: " + std::to_string(id_value));
            }
            const uint16_t id = static_cast<uint16_t>(id_value);
            AcsDestination dest;
            dest.id         = id;
            dest.ip_address = dest_node.get<std::string>("ip");
            dest.port       = get_port(dest_node, "port");
            cfg.destinations[id] = dest;
        }
    }

    return cfg;
}

NavsConfig parse_navs(const pt::ptree& root) {
    NavsConfig cfg;
    if (!root.get_child_optional("navs")) {
        return cfg;
    }

    cfg.enabled         = true;
    cfg.multicast_group = root.get<std::string>("navs.multicast_group");
    cfg.multicast_port  = get_port(root, "navs.multicast_port");

    if (const auto bindings = root.get_child_optional("navs.topic_binding")) {
        for (const auto& [key, binding_node] : *bindings) {
            NavsTopicBinding binding;
            binding.message_id = binding_node.get<uint32_t>("message_id");
            binding.topic      = binding_node.get<std::string>("topic");
            cfg.topic_bindings.push_back(binding);
        }
    }

    return cfg;
}

} // namespace

AppConfig loadAppConfig(const std::string& config_path) {
    pt::ptree root;
    try {
        pt::ini_parser::read_ini(config_path, root);
    } catch (const pt::ini_parser::ini_parser_error& e) {
        throw std::runtime_error("Errore nel file di configurazione: " + std::string(e.what()));
    }

    AppConfig cfg;
    cfg.cms  = parse_cms(root);
    cfg.acs  = parse_acs(root);
    cfg.navs = parse_navs(root);
    return cfg;
}
