#include "../include/crow_all.h"
#include <resources.h>
#include "FileManager.h"
#include <iostream>
#include <signal.h>
#include <memory>
#include <set>
#include <fstream>
#include <curl/curl.h>
#include <thread>
#include <chrono>

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
    std::string ip = req.get_header_value("X-Real-IP");
    if (ip.empty()) {
        ip = req.get_header_value("X-Forwarded-For");
    }
    if (ip.empty()) {
        ip = req.remote_ip_address;
    }
    return ip;
}

// Función para obtener la IP del servidor llamándose a sí mismo
std::string getServerIP() {
    try {
        // Hacer una petición HTTP al propio servidor
        CURL* curl = curl_easy_init();
        if (!curl) return "localhost";
        
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8081/api/my_ip");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
            +[](char* ptr, size_t size, size_t nmemb, std::string* data) {
                data->append(ptr, size * nmemb);
                return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
        
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res == CURLE_OK) {
            auto json = crow::json::load(response);
            if (json && json.has("ip")) {
                return json["ip"].s();
            }
        }
    } catch (...) {
        return "localhost";
    }
    return "localhost";
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    g_file_manager = std::make_unique<FileManager>();
    
    crow::SimpleApp app;

    // ============================================
    // WebSocket
    // ============================================
    CROW_ROUTE(app, "/ws")
    .websocket()
    .onopen([](crow::websocket::connection& conn){
        std::lock_guard<std::mutex> lock(g_ws_mutex);
        g_ws_connections.insert(&conn);
        std::cout << "🔌 Cliente conectado via WebSocket" << std::endl;
        
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
    
    CROW_ROUTE(app, "/api/my_ip")
    ([](const crow::request& req){
        crow::json::wvalue response;
        response["ip"] = getClientIP(req);
        return response;
    });
    
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

    // 🔥 DESCARGA CON STREAMING REAL - SOLUCIÓN
    CROW_ROUTE(app, "/api/download/<string>")
    ([](const crow::request& req, crow::response& res, const std::string& filename){
        std::string file_path = g_file_manager->getFilePath(filename);
        
        // Validar existencia
        if (!g_file_manager->fileExists(filename)) {
            res.code = 404;
            res.body = "File not found";
            res.end();
            return;
        }
        
        // Obtener tamaño del archivo
        struct stat stat_buf;
        if (stat(file_path.c_str(), &stat_buf) != 0) {
            res.code = 500;
            res.body = "Cannot stat file";
            res.end();
            return;
        }
        
        size_t file_size = stat_buf.st_size;
        
        // 📊 Log inicio de descarga
        std::cout << "⬇️  Iniciando descarga: " << filename 
                  << " (" << (file_size / 1024.0 / 1024.0) << " MB)" << std::endl;
        
        // Configurar headers ANTES de abrir el archivo
        res.set_header("Content-Type", "application/octet-stream");
        res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        res.set_header("Content-Length", std::to_string(file_size));
        res.set_header("Accept-Ranges", "bytes");
        res.set_header("Cache-Control", "no-cache");
        res.code = 200;
        
        // ✅ STREAMING REAL: Usar stream_response
        res.set_static_file_info_unsafe(file_path);
        
        // Alternativa manual si set_static_file_info_unsafe no funciona:
        /*
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            res.code = 500;
            res.body = "Cannot open file";
            res.end();
            return;
        }
        
        // Chunk de 256KB (óptimo para transferencia)
        const size_t chunk_size = 262144;
        std::vector<char> buffer(chunk_size);
        
        size_t total_sent = 0;
        
        while (file.read(buffer.data(), chunk_size) || file.gcount() > 0) {
            size_t bytes_read = file.gcount();
            
            // ✅ ENVIAR INMEDIATAMENTE cada chunk
            res.write(buffer.data(), bytes_read);
            
            total_sent += bytes_read;
            
            // Log progreso cada 10 MB
            if (total_sent % (10 * 1024 * 1024) == 0 || total_sent == file_size) {
                std::cout << "📤 Progreso: " 
                          << (total_sent / 1024.0 / 1024.0) << " / "
                          << (file_size / 1024.0 / 1024.0) << " MB ("
                          << (100 * total_sent / file_size) << "%)" << std::endl;
            }
        }
        
        file.close();
        std::cout << "✅ Descarga completa: " << filename << std::endl;
        */
        
        res.end();
    });

    // ============================================
    // Rutas estáticas SIN CACHÉ
    // ============================================
    
    CROW_ROUTE(app, "/")
    ([](){
        auto* res = Resources::getResource("/index.html");
        if (res) {
            auto response = crow::response(res->content);
            response.set_header("Content-Type", res->mime_type);
            response.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
            response.set_header("Pragma", "no-cache");
            response.set_header("Expires", "0");
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
            response.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
            response.set_header("Pragma", "no-cache");
            response.set_header("Expires", "0");
            return response;
        }
        
        return crow::response(404, "Resource not found: " + resource_path);
    });
    // ============================================
    std::cout << "🚀 AutoSync Server iniciando en puerto 8081..." << std::endl;
    std::cout << "📂 Directorio temporal: " << g_file_manager->getTempDir() << std::endl;
    std::cout << "⚠️  ADVERTENCIA: Todos los archivos se eliminarán al cerrar el servidor" << std::endl;
    std::cout << "\n🌐 Accede desde tu navegador:" << std::endl;
    std::cout << "   \033]8;;http://localhost:8081\033\\"
            << "http://localhost:8081"
            << "\033]8;;\033\\" << std::endl;

    // Iniciar servidor en un thread separado para poder hacer la petición
    std::thread server_thread([&app]() {
        app.port(8081).multithreaded().run();
    });

    // ✅ ESPERAR A QUE EL SERVIDOR ESTÉ REALMENTE LISTO
    std::cout << "⏳ Esperando a que el servidor inicie..." << std::endl;
    
    bool server_ready = false;
    int max_attempts = 20; // 10 segundos máximo (20 * 500ms)
    
    for (int attempt = 0; attempt < max_attempts && !server_ready; attempt++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        try {
            CURL* curl = curl_easy_init();
            if (curl) {
                std::string response;
                curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8081/api/status");
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
                    +[](char* ptr, size_t size, size_t nmemb, std::string* data) {
                        data->append(ptr, size * nmemb);
                        return size * nmemb;
                    });
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1L);
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 1L);
                
                CURLcode res = curl_easy_perform(curl);
                curl_easy_cleanup(curl);
                
                if (res == CURLE_OK) {
                    server_ready = true;
                    std::cout << "✅ Servidor listo (intento " << (attempt + 1) << ")" << std::endl;
                }
            }
        } catch (...) {
            // Ignorar errores y seguir intentando
        }
    }
    
    if (!server_ready) {
        std::cout << "⚠️  El servidor tardó en iniciar, continuando de todos modos..." << std::endl;
    }

    // Obtener IP llamándose a sí mismo
    std::string server_ip = getServerIP();
    if (server_ip != "localhost" && server_ip != "127.0.0.1") {
        std::cout << "   \033]8;;http://" << server_ip << ":8081\033\\"
                << "http://" << server_ip << ":8081"
                << "\033]8;;\033\\" << " (red local)" << std::endl;
    }

    std::cout << std::endl;

    server_thread.join();

    // Limpiar archivos temporales al finalizar
    g_file_manager->cleanup();

    return 0;
}