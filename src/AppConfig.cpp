#include "AppConfig.hpp"

#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <algorithm>
#include <cctype>
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

std::string trim_copy(std::string value) {
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::vector<std::string> parse_multicast_groups_list(const std::string& value) {
    std::vector<std::string> groups;
    std::size_t start = 0;

    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = (comma == std::string::npos) ? value.size() : comma;
        std::string token = trim_copy(value.substr(start, end - start));
        if (!token.empty()) {
            groups.push_back(std::move(token));
        }

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    return groups;
}

void parse_destination_node(const pt::ptree& dest_node, std::map<uint16_t, AcsDestination>& outDestinations) {
    const int id_value = dest_node.get<int>("id");
    if (id_value < 0 || id_value > 65535) {
        throw std::runtime_error("ID ACS non valido: " + std::to_string(id_value));
    }

    const uint16_t id = static_cast<uint16_t>(id_value);
    AcsDestination dest;
    dest.id = id;
    dest.ip_address = dest_node.get<std::string>("ip");
    dest.port = get_port(dest_node, "port");
    outDestinations[id] = dest;
}

void parse_destinations_from_root(const pt::ptree& root, std::map<uint16_t, AcsDestination>& outDestinations) {
    if (const auto groupedDests = root.get_child_optional("acs.destination")) {
        for (const auto& [key, dest_node] : *groupedDests) {
            (void)key;
            parse_destination_node(dest_node, outDestinations);
        }
        return;
    }

    const std::string flatPrefix = "acs.destination.";
    for (const auto& [sectionName, sectionNode] : root) {
        if (sectionName.rfind(flatPrefix, 0) == 0) {
            parse_destination_node(sectionNode, outDestinations);
        }
    }
}

std::vector<AcsDestination> extract_destinations_from_root(const pt::ptree& root) {
    std::vector<AcsDestination> destinations;

    auto append_if_valid = [&destinations](const pt::ptree& dest_node) {
        AcsDestination destination;

        if (const auto id = dest_node.get_optional<int>("id")) {
            if (*id < 0 || *id > 65535) {
                return;
            }
            destination.id = static_cast<uint16_t>(*id);
        }

        if (const auto ip = dest_node.get_optional<std::string>("ip")) {
            destination.ip_address = trim_copy(*ip);
        }

        if (const auto port = dest_node.get_optional<int>("port")) {
            if (*port >= 1 && *port <= 65535) {
                destination.port = static_cast<uint16_t>(*port);
            }
        }

        if (destination.id != 0 || !destination.ip_address.empty() || destination.port != 0) {
            destinations.push_back(std::move(destination));
        }
    };

    if (const auto groupedDests = root.get_child_optional("acs.destination")) {
        for (const auto& [key, dest_node] : *groupedDests) {
            (void)key;
            append_if_valid(dest_node);
        }
        return destinations;
    }

    const std::string flatPrefix = "acs.destination.";
    for (const auto& [sectionName, sectionNode] : root) {
        if (sectionName.rfind(flatPrefix, 0) == 0) {
            append_if_valid(sectionNode);
        }
    }

    return destinations;
}

CmsConfig parse_cms(const pt::ptree& root) {
    if (!root.get_child_optional("cms")) {
        throw std::runtime_error("Sezione 'cms' mancante o non valida");
    }

    CmsConfig cfg;
    cfg.multicast_port  = get_port(root, "cms.multicast_port");

    if (const auto single_group = root.get_optional<std::string>("cms.multicast_group")) {
        cfg.multicast_group = trim_copy(*single_group);
    }

    if (const auto groups = root.get_optional<std::string>("cms.multicast_groups")) {
        cfg.multicast_groups = parse_multicast_groups_list(*groups);
    }

    if (cfg.multicast_groups.empty() && !cfg.multicast_group.empty()) {
        cfg.multicast_groups.push_back(cfg.multicast_group);
    }

    if (cfg.multicast_groups.empty()) {
        throw std::runtime_error("Configurazione CMS non valida: specificare 'cms.multicast_group' o 'cms.multicast_groups'");
    }

    cfg.multicast_group = cfg.multicast_groups.front();

    return cfg;
}

AcsConfig parse_acs(const pt::ptree& root) {
    AcsConfig cfg;
    cfg.multicast_group    = "226.1.1.30";
    cfg.multicast_groups   = { cfg.multicast_group };
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

    if (const auto single_group = root.get_optional<std::string>("acs.multicast_group")) {
        cfg.multicast_group = trim_copy(*single_group);
    }
    cfg.multicast_groups.clear();
    if (const auto groups = root.get_optional<std::string>("acs.multicast_groups")) {
        cfg.multicast_groups = parse_multicast_groups_list(*groups);
    }
    if (cfg.multicast_groups.empty() && !cfg.multicast_group.empty()) {
        cfg.multicast_groups.push_back(cfg.multicast_group);
    }
    if (cfg.multicast_groups.empty()) {
        throw std::runtime_error("Configurazione ACS non valida: specificare 'acs.multicast_group' o 'acs.multicast_groups'");
    }
    cfg.multicast_group = cfg.multicast_groups.front();

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

    parse_destinations_from_root(root, cfg.destinations);

    return cfg;
}

std::vector<AcsDestination> extract_acs_destinations(const pt::ptree& root) {
    return extract_destinations_from_root(root);
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
    return cfg;
}

std::vector<AcsDestination> extractAcsDestination(const std::string& config_path) {
    pt::ptree root;
    try {
        pt::ini_parser::read_ini(config_path, root);
    } catch (const pt::ini_parser::ini_parser_error& e) {
        throw std::runtime_error("Errore nel file di configurazione: " + std::string(e.what()));
    }

    return extract_acs_destinations(root);
}
