#include "../include/crow_all.h"
#include <resources.h>  // Auto-generado por CMake en build/generated/
#include <iostream>

int main() {
    crow::SimpleApp app;

    // 🔥 API PRIMERO
    CROW_ROUTE(app, "/api/status")
    ([](){
        crow::json::wvalue status;
        status["status"] = "running";
        status["message"] = "AutoSync Server está activo";
        status["resources_loaded"] = Resources::RESOURCE_MAP.size();
        return status;
    });

    CROW_ROUTE(app, "/api/resources")
    ([](){
        crow::json::wvalue result;
        result["count"] = Resources::RESOURCE_MAP.size();

        std::vector<std::string> paths;
        for (const auto& [path, _] : Resources::RESOURCE_MAP) {
            paths.push_back(path);
        }
        result["paths"] = paths;
        return result;
    });

    // Página principal
    CROW_ROUTE(app, "/")
    ([](){
        auto* res = Resources::getResource("/index.html");
        if (res) {
            auto response = crow::response(res->content);
            response.set_header("Content-Type", res->mime_type);
            return response;
        }
        return crow::response(404);
    });

    // ⚠️ CATCH-ALL AL FINAL
    CROW_ROUTE(app, "/<path>")
    ([](const crow::request&, const std::string& path){
        std::string resource_path = "/" + path;

        auto* res = Resources::getResource(resource_path);
        if (res) {
            auto response = crow::response(res->content);
            response.set_header("Content-Type", res->mime_type);
            response.set_header("Cache-Control", "public, max-age=31536000");
            return response;
        }

        return crow::response(404, "Resource not found: " + resource_path);
    });

    app.port(8081).multithreaded().run();
}
