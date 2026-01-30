#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sys/stat.h>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

struct Message {
    std::string id;
    std::string type;  // "text" o "file"
    std::string content;  // texto o nombre del archivo
    std::string filename;  // solo para archivos
    size_t filesize;  // solo para archivos
    std::string timestamp;
    std::string sender_ip;
};

class FileManager {
private:
    std::string temp_dir;
    std::vector<Message> messages;
    std::mutex mtx;
    
    std::string generateId();
    std::string getCurrentTimestamp();
    void ensureTempDirExists();
    
public:
    FileManager();
    ~FileManager();
    
    // Gestión de mensajes
    std::string addTextMessage(const std::string& text, const std::string& sender_ip);
    std::string addFileMessage(const std::string& filename, const std::string& file_data, const std::string& sender_ip);
    
    // Obtener datos
    std::vector<Message> getAllMessages();
    std::string getFilePath(const std::string& filename);
    bool fileExists(const std::string& filename);
    
    // Limpieza
    void cleanup();
    std::string getTempDir() const { return temp_dir; }
};

#endif