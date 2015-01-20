#include "test/test.h"

#include "src/configuration.h"
#include "horsewhisperer/horsewhisperer.h"

extern std::string ROOT_PATH;

void configure_test() {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/test/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/test/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/test/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;
    int test_val = 0;

    CthunAgent::Configuration::Instance().initialize(argc, const_cast<char**>(argv));
}

TEST_CASE("Configuration::setStartFunction", "[configuration") {
    std::string server = "wss://test_server/";
    std::string ca = ROOT_PATH + "/test/resources/config/ca_crt.pem";
    std::string cert = ROOT_PATH +  "/test/resources/config/test_crt.pem";
    std::string key = ROOT_PATH + "/test/resources/config/test_key.pem";

    const char* argv[] = { "test-command", "--server", server.data(), "--ca", ca.data(),
                           "--cert", cert.data(), "--key", key.data() };
    int argc= 9;
    int test_val = 0;

    CthunAgent::Configuration::Instance().setStartFunction([&test_val] (std::vector<std::string> arg) -> int {
        return 1;
    });

    CthunAgent::Configuration::Instance().initialize(argc, const_cast<char**>(argv));

    REQUIRE(HorseWhisperer::Start() == 1);
}

TEST_CASE("Configuration::set", "[configuration") {
    configure_test();
    CthunAgent::Configuration::Instance().set<std::string>("ca", "value");
    REQUIRE(HorseWhisperer::GetFlag<std::string>("ca") == "value");
}

TEST_CASE("Configuration::get", "[configuration") {
    configure_test();
    HorseWhisperer::SetFlag<std::string>("ca", "value");
    REQUIRE(CthunAgent::Configuration::Instance().get<std::string>("ca") == "value");
}

TEST_CASE("Configuration::validateConfiguration", "[configuration") {
    configure_test();

    SECTION("it fails when server is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("server", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when server is invlaid") {
        CthunAgent::Configuration::Instance().set<std::string>("server", "ws://");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when ca is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("ca", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when ca file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("ca", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when cert is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("cert", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when cert file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("cert", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when key is undefined") {
        CthunAgent::Configuration::Instance().set<std::string>("key", "");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

    SECTION("it fails when key file cannot be found") {
        CthunAgent::Configuration::Instance().set<std::string>("key", "/fake/file");
        REQUIRE_THROWS_AS(CthunAgent::Configuration::Instance().validateConfiguration(0),
                          CthunAgent::required_not_set_error);
    }

}
