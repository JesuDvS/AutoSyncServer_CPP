#define _GNU_SOURCE

#include "FileManager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

FileManager::FileManager() {
    // Obtener el directorio del ejecutable
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
    
    if (len != -1) {
        buffer[len] = '\0';
        std::string exe_path(buffer);
        size_t last_slash = exe_path.find_last_of('/');
        std::string exe_dir = exe_path.substr(0, last_slash);
        temp_dir = exe_dir + "/temp_shared";
    } else {
        temp_dir = "./temp_shared";
    }
    
    ensureTempDirExists();
    std::cout << "📁 Directorio temporal: " << temp_dir << std::endl;
}

FileManager::~FileManager() {
    cleanup();
}

void FileManager::ensureTempDirExists() {
    if (!fs::exists(temp_dir)) {
        fs::create_directories(temp_dir);
        std::cout << "✅ Directorio temporal creado" << std::endl;
    }
}

std::string FileManager::generateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    return std::to_string(now) + "_" + std::to_string(dis(gen));
}

std::string FileManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string FileManager::addTextMessage(const std::string& text, const std::string& sender_ip) {
    std::lock_guard<std::mutex> lock(mtx);
    
    Message msg;
    msg.id = generateId();
    msg.type = "text";
    msg.content = text;
    msg.timestamp = getCurrentTimestamp();
    msg.sender_ip = sender_ip;
    msg.filesize = 0;
    
    messages.push_back(msg);
    
    std::cout << "💬 Mensaje de texto agregado: " << msg.id << std::endl;
    return msg.id;
}

std::string FileManager::addFileMessage(const std::string& filename, const std::string& file_data, const std::string& sender_ip) {
    std::lock_guard<std::mutex> lock(mtx);
    
    // Guardar archivo en disco
    std::string safe_filename = generateId() + "_" + filename;
    std::string file_path = temp_dir + "/" + safe_filename;
    
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs) {
        std::cerr << "❌ Error al crear archivo: " << file_path << std::endl;
        return "";
    }
    
    ofs.write(file_data.data(), file_data.size());
    ofs.close();
    
    Message msg;
    msg.id = generateId();
    msg.type = "file";
    msg.filename = safe_filename;
    msg.content = filename;  // nombre original
    msg.filesize = file_data.size();
    msg.timestamp = getCurrentTimestamp();
    msg.sender_ip = sender_ip;
    
    messages.push_back(msg);
    
    std::cout << "📎 Archivo guardado: " << safe_filename << " (" << file_data.size() << " bytes)" << std::endl;
    return msg.id;
}

std::vector<Message> FileManager::getAllMessages() {
    std::lock_guard<std::mutex> lock(mtx);
    return messages;
}

std::string FileManager::getFilePath(const std::string& filename) {
    return temp_dir + "/" + filename;
}

bool FileManager::fileExists(const std::string& filename) {
    return fs::exists(getFilePath(filename));
}

void FileManager::cleanup() {
    std::lock_guard<std::mutex> lock(mtx);
    
    if (fs::exists(temp_dir)) {
        std::cout << "🧹 Limpiando directorio temporal..." << std::endl;
        
        try {
            fs::remove_all(temp_dir);
            std::cout << "✅ Directorio temporal eliminado" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Error al limpiar: " << e.what() << std::endl;
        }
    }
    
    messages.clear();
    std::cout << "🗑️  Mensajes borrados de memoria" << std::endl;
}