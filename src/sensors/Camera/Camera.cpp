#include "Camera.hpp"
#include <iostream>
#include <sys/mman.h> // Essential for memory mapping
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <jpeglib.h>

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
    
    config->at(0).pixelFormat = formats::RGB888;    

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

    // Start hardware processing
    camera->start();
    
    // --- WARM UP LOOP ---
    // We capture multiple frames to let Auto-Exposure (AE) & AWB settle.
    // 30 frames is roughly 1 second, which is usually enough.
    const int warmupFrames = 30;

    std::cout << "[Camera] Warming up (AE/AWB convergence)..." << std::endl;

    for (int i = 0; i < warmupFrames; ++i) {
        // 1. Reset the flag so we can wait again
        {
            std::lock_guard<std::mutex> lock(cameraMutex);
            requestComplete = false;
        }

        // 2. Reuse the request (Essential!)
        // For the very first frame (i=0), the request is fresh.
        // For subsequent frames, we must tell libcamera we are reusing the buffers.
        if (i > 0) {
            request->reuse(Request::ReuseBuffers);
        }

        // 3. Queue the request
        camera->queueRequest(request.get());

        // 4. Wait for the hardware to finish this frame
        std::unique_lock<std::mutex> lock(cameraMutex);
        cameraCv.wait(lock, [this] { return requestComplete; });
        
        // (Optional) Print dots to show progress
        if (i % 5 == 0) std::cout << "." << std::flush;
    }
    std::cout << std::endl;

    std::cout << "[Camera] Capture finished." << std::endl;

    // Stop camera to save power
    camera->stop();

    // Now save the data from the LAST buffer (the fully exposed one)
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

// Helper function to compress RGB data to JPEG
void saveJpeg(const std::string& filename, void* data, int width, int height, int stride) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE * outfile;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(filename.c_str(), "wb")) == NULL) {
        std::cerr << "[Camera] Can't open " << filename << std::endl;
        return;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;     
    cinfo.in_color_space = JCS_EXT_RGB; // Standard RGB color space

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE); // Quality 90%
    jpeg_start_compress(&cinfo, TRUE);

    unsigned char* buffer = static_cast<unsigned char*>(data);

    while (cinfo.next_scanline < cinfo.image_height) {
        // stride is the actual width of the memory row (often aligned to 32/64 bytes)
        row_pointer[0] = &buffer[cinfo.next_scanline * stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    
    std::cout << "[Camera] Saved JPEG: " << filename << std::endl;
}


// Memory Mapping
// We have to map the Kernel's memory (DMA) into our User Space to read it.
void Camera::saveBufferToFile(const std::string& filepath, FrameBuffer *buffer) {
    const FrameBuffer::Plane &plane = buffer->planes()[0];
    int fd = plane.fd.get();
    size_t length = plane.length;
    
    void *data = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        std::cerr << "[Camera] mmap failed!" << std::endl;
        return;
    }

    // Get dimensions from the active configuration
    StreamConfiguration &streamConfig = config->at(0);
    int width = streamConfig.size.width;
    int height = streamConfig.size.height;
    int stride = streamConfig.stride; // Crucial! Memory width != Image width

    // Compress!
    saveJpeg(filepath, data, width, height, stride);

    munmap(data, length);
}

} // namespace horus