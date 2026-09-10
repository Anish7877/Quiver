#pragma once
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
namespace fs = std::filesystem;

class ProcessLock {
        public:
                ProcessLock(const ProcessLock&) = delete;
                ProcessLock& operator=(const ProcessLock&) = delete;
                ProcessLock(ProcessLock&&) = delete;
                ProcessLock& operator=(ProcessLock&&) = delete;

                explicit ProcessLock(const fs::path& lock_path) {
                        fs::create_directories(lock_path.parent_path());
                        m_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0666);
                        if (m_fd == -1) {
                                throw std::runtime_error("Failed to open lock file: " + lock_path.string());
                        }
                        if (flock(m_fd, LOCK_EX) == -1) {
                                close(m_fd);
                                throw std::runtime_error("Failed to acquire process lock.");
                        }
                }
                ~ProcessLock() {
                        if (m_fd != -1) {
                                flock(m_fd, LOCK_UN);
                                close(m_fd);
                        }
                }
        private:
                int m_fd{-1};
};
