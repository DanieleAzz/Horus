#include "Camera.hpp"
#include <iostream>
#include <sys/mman.h> // Essential for memory mapping
#include <unistd.h>
#include <fcntl.h>
#include <fstream>

namespace horus {

Camera::Camera() {
    cm = std::make_unique<CameraManager>();
    cm->start(); // Load the camera manager
}

Camera::~Camera() {
    stop();
    cm->stop();
}

bool Camera::start() {
    if (cm->cameras().empty()) {
        std::cerr << "[Camera] No cameras found." << std::endl;
        return false;
    }

    // 1. Acquire Camera (Grab the first one found)
    camera = cm->cameras()[0];
    if (camera->acquire()) {
        std::cerr << "[Camera] Failed to acquire lock." << std::endl;
        return false;
    }

    // 2. Configure: We want a Still Capture (High Res)
    config = camera->generateConfiguration({ StreamRole::StillCapture });
    
    // Force specific format if needed (e.g., NV12 is standard for Pi ISP)
    // config->at(0).pixelFormat = formats::NV12; 

    if (config->validate() == CameraConfiguration::Invalid) {
        std::cerr << "[Camera] Invalid configuration." << std::endl;
        return false;
    }

    if (camera->configure(config.get()) < 0) {
        std::cerr << "[Camera] Configuration failed." << std::endl;
        return false;
    }

    // 3. Allocate Buffers (Reserve RAM for the images)
    allocator = std::make_unique<FrameBufferAllocator>(camera);
    Stream *stream = config->at(0).stream();
    
    if (allocator->allocate(stream) < 0) {
        std::cerr << "[Camera] Buffer allocation failed." << std::endl;
        return false;
    }

    // 4. Create the Request object
    request = camera->createRequest();
    if (!request) {
        std::cerr << "[Camera] Failed to create request." << std::endl;
        return false;
    }

    // Assign the allocated buffer to the request
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
    if (request->addBuffer(stream, buffers[0].get()) < 0) {
        std::cerr << "[Camera] Failed to attach buffer." << std::endl;
        return false;
    }

    // 5. Connect the Signal (The "Callback")
    // When camera finishes, it calls 'requestCompleteHandler'
    camera->requestCompleted.connect(this, &Camera::requestCompleteHandler);

    return true;
}

void Camera::stop() {
    if (camera) {
        camera->stop();
        camera->release();
        camera.reset();
    }
}

// The "Main Event": This blocks until the photo is taken
bool Camera::capture(const std::string& filepath) {
    if (!camera) return false;

    // Reset flag
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        requestComplete = false;
    }

    // Start hardware processing
    camera->start();
    
    // Send the "Bucket" (Request) to the camera to be filled
    camera->queueRequest(request.get());
    
    std::cout << "[Camera] Request queued. Waiting for hardware..." << std::endl;

    // Wait here until the signal fires
    std::unique_lock<std::mutex> lock(cameraMutex);
    cameraCv.wait(lock, [this] { return requestComplete; });

    std::cout << "[Camera] Capture finished." << std::endl;

    // Stop camera to save power
    camera->stop();

    // Now save the data from the buffer
    Stream *stream = config->at(0).stream();
    FrameBuffer *buffer = request->buffers().at(stream);
    
    saveBufferToFile(filepath, buffer);
    
    return true;
}

// This runs in a separate thread managed by libcamera!
void Camera::requestCompleteHandler(Request *req) {
    if (req->status() == Request::RequestComplete) {
        std::lock_guard<std::mutex> lock(cameraMutex);
        requestComplete = true;
    }
    // Notify the main thread to wake up
    cameraCv.notify_one();
}

// HARDCORE C++: Memory Mapping
// We have to map the Kernel's memory (DMA) into our User Space to read it.
void Camera::saveBufferToFile(const std::string& filepath, FrameBuffer *buffer) {
    
    // A buffer can have multiple "planes" (e.g., Y color and UV color separated)
    // We will dump all planes to one file.
    std::ofstream outFile(filepath, std::ios::binary);
    
    for (const FrameBuffer::Plane &plane : buffer->planes()) {
        // Get the File Descriptor (fd) of the memory
        int fd = plane.fd.get();
        size_t length = plane.length;

        // mmap: Map the file descriptor to a pointer in our memory
        void *data = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
        
        if (data == MAP_FAILED) {
            std::cerr << "[Camera] mmap failed!" << std::endl;
            continue;
        }

        // Write to disk
        outFile.write(static_cast<char*>(data), length);

        // Clean up
        munmap(data, length);
    }
    
    outFile.close();
    std::cout << "[Camera] Raw data saved to " << filepath << std::endl;
}

} // namespace horus