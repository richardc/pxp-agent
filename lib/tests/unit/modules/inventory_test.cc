#include <cthun-agent/modules/inventory.hpp>
#include <cthun-agent/errors.hpp>

#include <cthun-client/protocol/chunks.hpp>       // ParsedChunks

#include <leatherman/json_container/json_container.hpp>

#include <catch.hpp>

#include <string>
#include <vector>
#include <algorithm>

namespace CthunAgent {

namespace lth_jc = leatherman::json_container;

static const std::string inventory_action { "inventory" };
static const std::string inventory_txt {
    "{  \"module\" : \"inventory\","
    "   \"action\" : \"inventory\","
    "   \"params\" : {}"
    "}"
};

static const std::vector<lth_jc::JsonContainer> no_debug {};
static const CthunClient::ParsedChunks parsed_chunks {
                    lth_jc::JsonContainer(),
                    lth_jc::JsonContainer(inventory_txt),
                    no_debug,
                    0 };

TEST_CASE("Modules::Inventory::callAction", "[modules]") {
    Modules::Inventory inventory_module {};
    ActionRequest request { RequestType::Blocking, parsed_chunks };

    SECTION("the inventory module is correctly named") {
        REQUIRE(inventory_module.module_name == "inventory");
    }

    SECTION("the inventory module has the inventory action") {
        auto found = std::find(inventory_module.actions.begin(),
                               inventory_module.actions.end(),
                               inventory_action);
        REQUIRE(found != inventory_module.actions.end());
    }

    SECTION("it can call the inventory action") {
        REQUIRE_NOTHROW(inventory_module.executeAction(request));
    }

    SECTION("it should execute the inventory action correctly") {
        auto outcome = inventory_module.executeAction(request);
        CHECK(outcome.results.toString().find("facts") != std::string::npos);
    }
}

}  // namespace CthunAgent
