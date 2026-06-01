#include <atomic>
#include <civetweb.h>
#include <iostream>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <vector>

// Global flag for controlling the camera
std::atomic<bool> isRunning(false);
cv::VideoCapture videoCap;
cv::Mat currentFrame;
std::mutex frameMutex;

// HTML page as a string
const char *htmlPage = R"(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenCV Camera View</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f0f0f0;
            text-align: center;
        }
        h1 { color: #333; }
        #videoContainer {
            background: #fff;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            padding: 20px;
            display: inline-block;
        }
        #cameraFeed {
            max-width: 100%;
            width: 640px;
            height: 480px;
            border: 2px solid #333;
            border-radius: 4px;
        }
        #status {
            margin-top: 10px;
            padding: 10px;
            border-radius: 4px;
        }
        .status-ok { background-color: #d4edda; color: #155724; }
        .status-error { background-color: #f8d7da; color: #721c24; }
    </style>
</head>
<body>
    <h1>OpenCV Camera Stream</h1>
    <div id="videoContainer">
        <img id="cameraFeed" src="/video_feed" alt="Camera Feed">
        <div id="status" class="status-ok">Streaming</div>
    </div>

    <script>
        setInterval(function() {
            var img = document.getElementById('cameraFeed');
            img.src = '/video_feed?' + new Date().getTime();
        }, 100);
    </script>
</body>
</html>
)";

// Handle HTTP requests
int handleRequest(struct mg_connection *conn) {
  const struct mg_request_info *reqInfo = mg_get_request_info(conn);
  if (!reqInfo)
    return 0;

  std::string uri = reqInfo->local_uri ? reqInfo->local_uri : "";

  // Route: Main page
  if (uri == "/" || uri == "") {
    mg_printf(conn,
              "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
              "%zu\r\n\r\n",
              strlen(htmlPage));
    mg_write(conn, htmlPage, strlen(htmlPage));
    return 200;
  }

  // Route: Video stream
  if (uri == "/video_feed") {
    // Capture frame in main thread
    if (videoCap.isOpened()) {
      cv::Mat frame;
      videoCap >> frame;
      if (!frame.empty()) {
        std::unique_lock<std::mutex> lock(frameMutex);
        currentFrame = frame.clone();
      }
    }

    std::unique_lock<std::mutex> lock(frameMutex);
    if (!currentFrame.empty()) {
      std::vector<uchar> buf;
      cv::imencode(".jpg", currentFrame, buf, {cv::IMWRITE_JPEG_QUALITY, 80});
      std::string header = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: image/jpeg\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Content-Length: " +
                           std::to_string(buf.size()) +
                           "\r\n"
                           "\r\n";
      mg_write(conn, header.c_str(), header.length());
      mg_write(conn, buf.data(), buf.size());
      return 200;
    }
    const char *err = "No frame available";
    mg_printf(conn,
              "HTTP/1.1 404 Not Found\r\nContent-Type: "
              "text/plain\r\nContent-Length: %zu\r\n\r\n",
              strlen(err));
    mg_write(conn, err, strlen(err));
    return 404;
  }

  // 404 for unknown routes
  const char *notFound = "Not Found";
  mg_printf(conn,
            "HTTP/1.1 404 Not Found\r\nContent-Type: "
            "text/plain\r\nContent-Length: %zu\r\n\r\n",
            strlen(notFound));
  mg_write(conn, notFound, strlen(notFound));
  return 404;
}

int main() {
    std::cout << "OpenCV version: " << CV_VERSION << std::endl;

    // Open camera in main thread
    std::cout << "Opening camera..." << std::endl;
    videoCap.open(0);
    if (!videoCap.isOpened()) {
      std::cerr << "Error: Cannot open camera" << std::endl;
      return 1;
    }
    std::cout << "Camera opened!" << std::endl;

    videoCap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    videoCap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    videoCap.set(cv::CAP_PROP_FPS, 30);
    std::cout << "Resolution: " << videoCap.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
              << videoCap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;

    isRunning = true;

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = handleRequest;

    const char *options[] = {
        "document_root", ".", "listening_ports", "8088", "request_timeout_ms",
        "10000",         NULL};

    struct mg_init_data init;
    memset(&init, 0, sizeof(init));
    init.callbacks = &callbacks;
    init.user_data = NULL;
    init.configuration_options = options;

    struct mg_error_data error;
    memset(&error, 0, sizeof(error));
    char error_buf[512];
    error.text = error_buf;
    error.text_buffer_size = sizeof(error_buf);

    std::cout << "Starting server on http://localhost:8088" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    mg_context *ctx = mg_start2(&init, &error);
    if (ctx == NULL) {
      std::cerr << "Error: Cannot start server" << std::endl;
      if (error.text && error.text_buffer_size > 0) {
        std::cerr << "Reason: " << error.text << std::endl;
      }
      isRunning = false;
      return 1;
    }

    std::cout << "Server started. Open http://localhost:8088 in your browser."
              << std::endl;
    while (isRunning) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    videoCap.release();
    std::cout << "Camera released" << std::endl;

    mg_stop(ctx);
    std::cout << "Server stopped" << std::endl;
    return 0;
}