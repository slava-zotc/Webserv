#ifndef CONFIG_BUILDER_HPP
#define CONFIG_BUILDER_HPP

#include "ConfigParser.hpp"
#include "Route.hpp"
#include "Server.hpp"

CFG_Server buildCfgServer(const ConfigBlock& block);
CFG_Route  buildCfgRoute(const ConfigBlock& block, const std::string& parentRoot);

#endif
