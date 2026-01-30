#include "../include/crow_all.h"
#include <resources.h>
#include "FileManager.h"
#include <iostream>
#include <signal.h>
#include <memory>
#include <set>
#include <fstream>

// Variable global para limpieza
std::unique_ptr<FileManager> g_file_manager;
std::set<crow::websocket::connection*> g_ws_connections;
std::mutex g_ws_mutex;

void signalHandler(int signum) {
    std::cout << "\n🛑 Señal de interrupción recibida (" << signum << ")" << std::endl;
    
    if (g_file_manager) {
        g_file_manager->cleanup();
    }
    
    exit(signum);
}

void broadcastToAllClients(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    for (auto* conn : g_ws_connections) {
        conn->send_text(message);
    }
}

std::string getClientIP(const crow::request& req) {
    // Intentar obtener IP real del cliente
    std::string ip = req.get_header_value("X-Real-IP");
    if (ip.empty()) {
        ip = req.get_header_value("X-Forwarded-For");
    }
    if (ip.empty()) {
        ip = req.remote_ip_address;
    }
    return ip;
}

int main() {
    // Configurar manejadores de señales
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Inicializar FileManager
    g_file_manager = std::make_unique<FileManager>();
    
    crow::SimpleApp app;

    // ============================================
    // WebSocket para sincronización en tiempo real
    // ============================================
    CROW_ROUTE(app, "/ws")
    .websocket()
    .onopen([](crow::websocket::connection& conn){
        std::lock_guard<std::mutex> lock(g_ws_mutex);
        g_ws_connections.insert(&conn);
        std::cout << "🔌 Cliente conectado via WebSocket" << std::endl;
        
        // Enviar estado inicial
        auto messages = g_file_manager->getAllMessages();
        crow::json::wvalue response;
        response["type"] = "initial_state";
        response["messages"] = crow::json::wvalue::list();
        
        for (size_t i = 0; i < messages.size(); i++) {
            auto& msg = messages[i];
            crow::json::wvalue m;
            m["id"] = msg.id;
            m["type"] = msg.type;
            m["content"] = msg.content;
            m["timestamp"] = msg.timestamp;
            m["sender_ip"] = msg.sender_ip;
            
            if (msg.type == "file") {
                m["filename"] = msg.filename;
                m["filesize"] = msg.filesize;
            }
            
            response["messages"][i] = std::move(m);
        }
        
        conn.send_text(response.dump());
    })
    .onclose([](crow::websocket::connection& conn, const std::string&){
        std::lock_guard<std::mutex> lock(g_ws_mutex);
        g_ws_connections.erase(&conn);
        std::cout << "🔌 Cliente desconectado" << std::endl;
    })
    .onmessage([](crow::websocket::connection&, const std::string& data, bool){
        std::cout << "📨 Mensaje WebSocket recibido: " << data << std::endl;
    });

    // ============================================
    // API REST
    // ============================================
    
    // Estado del servidor
    CROW_ROUTE(app, "/api/status")
    ([](){
        crow::json::wvalue status;
        status["status"] = "running";
        status["message"] = "AutoSync Server está activo";
        status["resources_loaded"] = Resources::RESOURCE_MAP.size();
        status["total_messages"] = g_file_manager->getAllMessages().size();
        status["temp_dir"] = g_file_manager->getTempDir();
        return status;
    });

    // Obtener todos los mensajes
    CROW_ROUTE(app, "/api/messages")
    ([](){
        auto messages = g_file_manager->getAllMessages();
        crow::json::wvalue response;
        response["messages"] = crow::json::wvalue::list();
        
        for (size_t i = 0; i < messages.size(); i++) {
            auto& msg = messages[i];
            crow::json::wvalue m;
            m["id"] = msg.id;
            m["type"] = msg.type;
            m["content"] = msg.content;
            m["timestamp"] = msg.timestamp;
            m["sender_ip"] = msg.sender_ip;
            
            if (msg.type == "file") {
                m["filename"] = msg.filename;
                m["filesize"] = msg.filesize;
            }
            
            response["messages"][i] = std::move(m);
        }
        
        return response;
    });

    // Enviar mensaje de texto
    CROW_ROUTE(app, "/api/send_text")
    .methods("POST"_method)
    ([](const crow::request& req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("text")) {
            return crow::response(400, "Missing 'text' field");
        }
        
        std::string text = body["text"].s();
        std::string sender_ip = getClientIP(req);
        
        std::string msg_id = g_file_manager->addTextMessage(text, sender_ip);
        
        // Notificar a todos los clientes via WebSocket
        crow::json::wvalue notification;
        notification["type"] = "new_message";
        notification["message"]["id"] = msg_id;
        notification["message"]["type"] = "text";
        notification["message"]["content"] = text;
        notification["message"]["sender_ip"] = sender_ip;
        notification["message"]["timestamp"] = g_file_manager->getAllMessages().back().timestamp;
        
        broadcastToAllClients(notification.dump());
        
        crow::json::wvalue response;
        response["success"] = true;
        response["message_id"] = msg_id;
        return crow::response(response);
    });

    // Subir archivo
    CROW_ROUTE(app, "/api/upload")
    .methods("POST"_method)
    ([](const crow::request& req){
        crow::multipart::message msg(req);
        
        auto file_part = msg.get_part_by_name("file");
        if (!file_part.body.empty()) {
            auto headers = file_part.get_header_object("Content-Disposition");
            std::string filename = headers.params.at("filename");
            
            std::string sender_ip = getClientIP(req);
            std::string msg_id = g_file_manager->addFileMessage(filename, file_part.body, sender_ip);
            
            auto messages = g_file_manager->getAllMessages();
            auto& last_msg = messages.back();
            
            // Notificar a todos los clientes
            crow::json::wvalue notification;
            notification["type"] = "new_message";
            notification["message"]["id"] = msg_id;
            notification["message"]["type"] = "file";
            notification["message"]["content"] = last_msg.content;
            notification["message"]["filename"] = last_msg.filename;
            notification["message"]["filesize"] = last_msg.filesize;
            notification["message"]["sender_ip"] = sender_ip;
            notification["message"]["timestamp"] = last_msg.timestamp;
            
            broadcastToAllClients(notification.dump());
            
            crow::json::wvalue response;
            response["success"] = true;
            response["message_id"] = msg_id;
            response["filename"] = last_msg.filename;
            return crow::response(response);
        }
        
        return crow::response(400, "No file uploaded");
    });

    // Descargar archivo
    CROW_ROUTE(app, "/api/download/<string>")
    ([](const std::string& filename){
        std::string file_path = g_file_manager->getFilePath(filename);
        
        if (!g_file_manager->fileExists(filename)) {
            return crow::response(404, "File not found");
        }
        
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return crow::response(500, "Cannot read file");
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        auto response = crow::response(content);
        response.set_header("Content-Type", "application/octet-stream");
        response.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        return response;
    });

    // ============================================
    // Rutas estáticas (HTML, CSS, JS)
    // ============================================
    
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

    std::cout << "🚀 AutoSync Server iniciando en puerto 8081..." << std::endl;
    std::cout << "📂 Directorio temporal: " << g_file_manager->getTempDir() << std::endl;
    std::cout << "⚠️  ADVERTENCIA: Todos los archivos se eliminarán al cerrar el servidor" << std::endl;
    
    app.port(8081).multithreaded().run();
    
    // Limpieza al salir normalmente
    g_file_manager->cleanup();
    
    return 0;
}