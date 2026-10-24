#pragma once
#include <fstream>
#include <filesystem>
#include <iostream>
#include <mutex>

#ifndef MINLOG_OUT_FILE
#define MINLOG_OUT_FILE "T7Overclocked.log"
#endif

#ifndef MINLOG_OUT_DIR
#define MINLOG_OUT_DIR  "logs"
#endif

class MinLog
{
private:
    std::ofstream OutputStream;
    bool _isOpen = false;
    std::mutex OutputMutex;

public:
    static MinLog& Instance()
    {
        static MinLog _instance = MinLog(MINLOG_OUT_FILE, MINLOG_OUT_DIR);
        return _instance;
    }

    MinLog() { }

    MinLog(const char* filename, const char* dir = "")
    {
        Open(filename, dir);
    }

    void Open(const char* filename, const char* dir = "")
    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        if (_isOpen) return;
        std::error_code error;
        const auto current_directory = std::filesystem::current_path(error);
        if (error)
        {
            std::cerr << "Failed to locate log directory: " << error.message() << std::endl;
            return;
        }
        std::filesystem::path const directory = current_directory / dir;
        std::filesystem::create_directories(directory, error);
        if (error)
        {
            std::cerr << "Failed to create log directory: " << error.message() << std::endl;
            return;
        }
        MinLog::OutputStream = std::ofstream(directory / filename);
        _isOpen = OutputStream.is_open();
        if (!_isOpen)
        {
            std::cerr << "Failed to open log file: " << (directory / filename).string() << std::endl;
        }
    }

    void Write(const char* value)
    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        MinLog::OutputStream << value;
        std::cout << value;
    }

    void WriteLine(const char* value)
    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        MinLog::OutputStream << value << std::endl;
        std::cout << value << std::endl;
    }

    void WriteDebug(const char* value)
    {
#ifdef DEBUG
        std::lock_guard<std::mutex> lock(OutputMutex);
        MinLog::OutputStream << value << std::endl;
        std::cout << value << std::endl;
#endif
    }
};
