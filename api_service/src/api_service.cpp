#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <string>
#include <cstdlib> // For getenv
#include <jwt-cpp/jwt.h>
#include <curl/curl.h> // For making HTTP requests to Keycloak
#include "nlohmann/json.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// Callback function to write received data from curl
size_t write_callback(char *ptr, size_t size, size_t nmemb, std::string *userdata) {
    size_t total_size = size * nmemb;
    userdata->append(ptr, total_size);
    return total_size;
}

// Function to validate JWT token against Keycloak
bool validate_token(const std::string& token) {
    // Initialize curl
    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl." << std::endl;
        return false;
    }

    // Get environment variables
    const char* keycloak_url = std::getenv("APP_KEYCLOAK_URL");
    const char* keycloak_realm = curl_easy_escape(curl,std::getenv("APP_KEYCLOAK_REALM"), 0);
    const char* keycloak_client_id = curl_easy_escape(curl,std::getenv("APP_KEYCLOAK_CLIENT_ID"), 0);
    const char* keycloak_client_secret = curl_easy_escape(curl, std::getenv("APP_KEYCLOAK_CLIENT_SECRET"), 0);

    if (!keycloak_url || !keycloak_realm || !keycloak_client_id || !keycloak_client_secret) {
        std::cerr << "Missing Keycloak environment variables." << std::endl;
        return false; // Treat missing variables as invalid
    }

    // Construct introspection endpoint URL
    std::string introspection_url = std::string(keycloak_url) + "/realms/" + std::string(keycloak_realm) +
                                    "/protocol/openid-connect/token/introspect";

    // Prepare data for introspection request
    std::string post_data = "token=" + token + "&client_id=" + std::string(keycloak_client_id) +
                            "&client_secret=" + std::string(keycloak_client_secret);

    std::string response_data;

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, introspection_url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    // Parse JSON response
    try {
        json response_json = json::parse(response_data);
        if (response_json.contains("active") && response_json["active"].get<bool>() &&
            response_json.contains("realm_access") && response_json["realm_access"].contains("roles")) {
            auto roles = response_json["realm_access"]["roles"];
            for (const auto &role: roles) {
                if (role.get<std::string>() == "prothetic_user") {
                    curl_easy_cleanup(curl);
                    return true;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON or accessing fields: " << e.what() << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    return false;
}


// Handles an HTTP request
void handle_request(tcp::socket& socket) {
    beast::flat_buffer buffer;
    http::request<http::string_body> req;
    http::read(socket, buffer, req);

    http::response<http::string_body> res;
    res.version(req.version());
    res.keep_alive(req.keep_alive());
    res.insert(http::field::access_control_allow_origin, "*");
    res.insert(http::field::content_type, "application/json");

    if (req.method() == http::verb::options && req.target() == "/reports") {
        res.result(http::status::ok);
        res.insert(http::field::access_control_allow_methods, "GET");
        res.insert(http::field::access_control_allow_headers, "authorization");
    } else if (req.method() == http::verb::get && req.target() == "/reports") {
        // Extract JWT token from Authorization header
        std::string auth_header;
        for (const auto& field : req) {
            if (field.name() == http::field::authorization) {
                auth_header = field.value();
                break;
            }
        }

        std::string token;
        if (auth_header.size() > 7 && auth_header.substr(0, 7) == "Bearer ") {
            token = auth_header.substr(7);
            if (validate_token(token)) {
                res.result(http::status::ok);
                res.body() = "Under construction";
            } else {
                res.result(http::status::forbidden);
                res.body() = "Forbidden: Invalid token or missing role";
            }
        } else {
            res.result(http::status::forbidden);
            res.body() = "Missing or malformed Authorization header";
        }
    } else {
        res.result(http::status::not_found);
        res.body() = "Not Found";
    }

    res.content_length(res.body().size());
    res.prepare_payload();
    http::write(socket, res);
}


int main() {
    try {
        // Initialize curl globally (must be done before any curl functions are used)
        curl_global_init(CURL_GLOBAL_DEFAULT);

        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {tcp::v4(), 8080}};
        std::cout << "Listening on http://localhost:8080/reports" << std::endl;

        while (true) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            handle_request(socket);
            socket.shutdown(tcp::socket::shutdown_send);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        curl_global_cleanup(); // Cleanup curl
        return 1;
    }
    return 0;
}
