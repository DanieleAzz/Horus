#pragma once

#include <libcamera/libcamera.h>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>

namespace horus {

using namespace libcamera;

class Camera {
public:
    Camera();
    ~Camera();

    // Setup the camera hardware
    bool start();
    
    // The main blocking call: Takes a photo and saves raw data
    // Returns true on success
    bool capture(const std::string& filepath);

    // Shutdown
    void stop();

private:
    std::unique_ptr<CameraManager> cm;
    std::shared_ptr<libcamera::Camera> camera;
    std::unique_ptr<CameraConfiguration> config;
    std::unique_ptr<FrameBufferAllocator> allocator;
    std::vector<std::unique_ptr<Request>> requests;
    std::unique_ptr<Request> request;

    // Concurrency tools to wait for the hardware
    std::mutex cameraMutex;
    std::condition_variable cameraCv;
    bool requestComplete = false;

    // The callback function called by libcamera when image is ready
    void requestCompleteHandler(Request *request);
    
    // Helper to map hardware memory to CPU memory
    void saveBufferToFile(const std::string& filepath, FrameBuffer *buffer);
};

} // namespace horus