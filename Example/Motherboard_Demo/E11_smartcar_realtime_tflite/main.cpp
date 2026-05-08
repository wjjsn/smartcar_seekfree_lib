#include "zf_common_headfile.h"
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
using namespace cv;

#define MODEL_INPUT_WIDTH         96
#define MODEL_INPUT_HEIGHT       96
#define MODEL_INPUT_CHANNEL      3
#define MODEL_INPUT_SIZE         (MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT * MODEL_INPUT_CHANNEL)
#define MODEL_OUTPUT_CLASS_NUM   3
#define TFLITE_OP_RESOLVER_MAX_NUM 100
#define TENSOR_ARENA_SIZE        (1024 * 1024)

const char* class_labels[] = {"交通工具-直行", "武器-左", "物资-右"};

static Mat resize_with_lanczos(const Mat& src, int width, int height) {
    Mat dst;
    resize(src, dst, Size(width, height), INTER_LANCZOS4);
    return dst;
}

static Mat rotate_180(const Mat& src) {
    Mat dst;
    flip(src, dst, -1);
    return dst;
}

struct A4DetectionResult {
    bool success;
    Point2f tl, tr, br, bl;
    Point2f pt_top_left, pt_top_right;
    std::vector<Point> contour;
    std::string message;
};

static Mat create_red_mask(const Mat& hsv, const Scalar& lower_red1, const Scalar& upper_red1,
                           const Scalar& lower_red2, const Scalar& upper_red2) {
    Mat mask1, mask2;
    inRange(hsv, lower_red1, upper_red1, mask1);
    inRange(hsv, lower_red2, upper_red2, mask2);
    return mask1 | mask2;
}

static Mat apply_morphology(const Mat& mask, int kernel_size = 3) {
    Mat kernel = getStructuringElement(MORPH_RECT, Size(kernel_size, kernel_size));
    Mat result;
    morphologyEx(mask, result, MORPH_CLOSE, kernel);
    morphologyEx(result, result, MORPH_OPEN, kernel);
    return result;
}

static std::vector<Point> find_corner_points(const std::vector<Point>& points) {
    int min_sum_idx = 0, max_sum_idx = 0;
    int min_diff_idx = 0, max_diff_idx = 0;
    float min_sum_val = points[0].x + points[0].y;
    float max_sum_val = min_sum_val;
    float min_diff_val = points[0].x - points[0].y;
    float max_diff_val = min_diff_val;

    for (size_t i = 1; i < points.size(); i++) {
        float s = points[i].x + points[i].y;
        float d = points[i].x - points[i].y;
        if (s < min_sum_val) { min_sum_val = s; min_sum_idx = i; }
        if (s > max_sum_val) { max_sum_val = s; max_sum_idx = i; }
        if (d < min_diff_val) { min_diff_val = d; min_diff_idx = i; }
        if (d > max_diff_val) { max_diff_val = d; max_diff_idx = i; }
    }

    std::vector<Point> corners(4);
    corners[0] = points[min_sum_idx];
    corners[1] = points[max_sum_idx];
    corners[2] = points[min_diff_idx];
    corners[3] = points[max_diff_idx];
    return corners;
}

static void compute_top_edge_points(const Point2f* src_pts, float phys_width, float phys_img_height,
                                    float phys_red_height, Point2f& pt_top_left, Point2f& pt_top_right) {
    float w = phys_width, h_img = phys_img_height, h_red = phys_red_height;
    Point2f dst_pts[4] = {
        Point2f(0, h_img),
        Point2f(w, h_img),
        Point2f(w, h_img + h_red),
        Point2f(0, h_img + h_red)
    };

    Mat M = getPerspectiveTransform(dst_pts, src_pts);
    std::vector<Point2f> top_points;
    top_points.push_back(Point2f(0, 0));
    top_points.push_back(Point2f(w, 0));
    std::vector<Point2f> top_points_image;
    perspectiveTransform(top_points, top_points_image, M);
    pt_top_left = top_points_image[0];
    pt_top_right = top_points_image[1];
}

static Mat perspective_crop(const Mat& img, const Point2f* corners, const Point2f& pt_top_left, const Point2f& pt_top_right) {
    Point2f src_quad[4] = { corners[0], corners[1], corners[2], corners[3] };
    float width = norm(corners[1] - corners[0]);
    float height = norm(pt_top_left - corners[0]);
    Point2f dst_quad[4] = {
        Point2f(0, 0),
        Point2f(width, 0),
        Point2f(width, height),
        Point2f(0, height)
    };
    Mat M_warp = getPerspectiveTransform(src_quad, dst_quad);
    Mat warped;
    warpPerspective(img, warped, M_warp, Size((int)width, (int)height));
    return warped;
}

static A4DetectionResult detect_a4_points(const Mat& img) {
    A4DetectionResult result;
    result.success = false;

    Mat hsv;
    cvtColor(img, hsv, COLOR_BGR2HSV);

    Scalar lower_red1(0, 100, 100), upper_red1(15, 255, 255);
    Scalar lower_red2(150, 100, 100), upper_red2(180, 255, 255);

    Mat mask = create_red_mask(hsv, lower_red1, upper_red1, lower_red2, upper_red2);
    mask = apply_morphology(mask);

    std::vector<std::vector<Point>> contours;
    findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        result.message = "No red region found";
        return result;
    }

    int w = img.cols;
    float center_ratio = 0.5f;
    float min_area = 1000;
    int min_width = 40, min_height = 20;
    float center_left = (0.5f - center_ratio / 2) * w;
    float center_right = (0.5f + center_ratio / 2) * w;

    std::vector<std::tuple<std::vector<Point>, double, Rect>> center_contours;
    for (auto& cnt : contours) {
        double area = contourArea(cnt);
        Rect bounding = boundingRect(cnt);
        float center_x = bounding.x + bounding.width / 2.0f;
        if (center_x > center_left && center_x < center_right &&
            area > min_area && bounding.width > min_width && bounding.height > min_height) {
            center_contours.emplace_back(cnt, area, bounding);
        }
    }

    if (center_contours.empty()) {
        result.message = "No center red region found";
        return result;
    }

    std::sort(center_contours.begin(), center_contours.end(),
              [](const auto& a, const auto& b) { return std::get<double>(a) > std::get<double>(b); });

    std::vector<Point> best_cnt = std::get<std::vector<Point>>(center_contours[0]);
    std::vector<Point> hull;
    convexHull(best_cnt, hull);

    std::vector<Point> corners = find_corner_points(hull);
    result.tl = corners[0];
    result.tr = corners[2];
    result.br = corners[1];
    result.bl = corners[3];
    result.contour = hull;

    Point2f src_pts[4] = { result.tl, result.tr, result.br, result.bl };
    compute_top_edge_points(src_pts, 12.0f, 12.0f, 5.0f, result.pt_top_left, result.pt_top_right);

    result.success = true;
    return result;
}

static Mat preprocess_for_model(const Mat& warped) {
    Mat resized;
    resize(warped, resized, Size(MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT), INTER_LANCZOS4);
    resized.convertTo(resized, CV_32FC3, 1.0f / 127.5f);
    subtract(resized, Scalar(1.0f, 1.0f, 1.0f), resized);
    cvtColor(resized, resized, COLOR_BGR2RGB);
    return resized;
}

static void draw_detection(Mat& result, const A4DetectionResult& detection) {
    if (!detection.success) return;

    polylines(result, detection.contour, true, Scalar(255, 0, 0), 1);

    circle(result, detection.tl, 3, Scalar(0, 255, 0), -1);
    circle(result, detection.tr, 3, Scalar(0, 255, 0), -1);
    circle(result, detection.br, 3, Scalar(0, 255, 0), -1);
    circle(result, detection.bl, 3, Scalar(0, 255, 0), -1);

    line(result, detection.pt_top_left, detection.pt_top_right, Scalar(0, 0, 255), 2);
    circle(result, detection.pt_top_left, 3, Scalar(0, 0, 255), -1);
    circle(result, detection.pt_top_right, 3, Scalar(0, 0, 255), -1);
}

int main(int, char**) {
    fprintf(stderr, "E11 SmartCar Realtime TFLite Test Program\n");
    fprintf(stderr, "============================================\n");

    const char* model_path = "smartcar_model_tflm.tflite";

    int fd = open(model_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Failed to open model file: %s\n", model_path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "ERROR: Failed to stat model file\n");
        close(fd);
        return -1;
    }
    size_t model_size = st.st_size;

    void* model_buffer = mmap(NULL, model_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (model_buffer == MAP_FAILED) {
        fprintf(stderr, "ERROR: Failed to mmap model file\n");
        close(fd);
        return -1;
    }
    close(fd);
    fprintf(stderr, "Model loaded: %zu bytes at %p\n", model_size, model_buffer);

    tflite::InitializeTarget();

    const tflite::Model* model = ::tflite::GetModel(model_buffer);
    if (!model) {
        fprintf(stderr, "ERROR: Failed to parse model\n");
        munmap(model_buffer, model_size);
        return -1;
    }
    fprintf(stderr, "Model parsed successfully, version: %d\n", model->version());

    tflite::MicroMutableOpResolver<TFLITE_OP_RESOLVER_MAX_NUM> resolver;

    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddRelu6();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddShape();
    resolver.AddSlice();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddCast();
    resolver.AddSqueeze();
    resolver.AddExpandDims();
    resolver.AddConcatenation();
    resolver.AddTranspose();
    resolver.AddStridedSlice();
    resolver.AddPack();
    resolver.AddRelu();

    void* arena_raw = nullptr;
    if (posix_memalign(&arena_raw, 64, TENSOR_ARENA_SIZE) != 0) {
        fprintf(stderr, "ERROR: Failed to allocate tensor arena\n");
        munmap(model_buffer, model_size);
        return -1;
    }
    uint8_t* tensor_arena = (uint8_t*)arena_raw;

    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
    TfLiteStatus status = interpreter.AllocateTensors();

    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: AllocateTensors failed with error code %d\n", status);
        munmap(model_buffer, model_size);
        free(arena_raw);
        return -1;
    }

    fprintf(stderr, "Tensor allocation successful, arena used: %zu bytes\n", interpreter.arena_used_bytes());

    if (uvc_camera_init("/dev/video0") < 0) {
        fprintf(stderr, "ERROR: Failed to init camera\n");
        munmap(model_buffer, model_size);
        free(arena_raw);
        return -1;
    }
    fprintf(stderr, "Camera initialized successfully\n");

    fprintf(stderr, "Press 'q' to quit\n");

    int frame_count = 0;
    while (true) {
        frame_count++;

        if (wait_image_refresh() < 0) {
            fprintf(stderr, "Frame %d: camera error\n", frame_count);
            break;
        }

        if (frame_rgb.empty()) {
            fprintf(stderr, "Frame %d: empty frame\n", frame_count);
            continue;
        }

        Mat annotated = frame_rgb.clone();
        A4DetectionResult detection = detect_a4_points(frame_rgb);

        if (detection.success) {
            Point2f corners[4] = { detection.tl, detection.tr, detection.br, detection.bl };
            Mat warped = perspective_crop(frame_rgb, corners, detection.pt_top_left, detection.pt_top_right);
            warped = rotate_180(warped);

            Mat preprocessed = preprocess_for_model(warped);
            Mat continuous_img = preprocessed.isContinuous() ? preprocessed : preprocessed.clone();

            float* input_data = interpreter.input(0)->data.f;
            memcpy(input_data, continuous_img.ptr<float>(), MODEL_INPUT_SIZE * sizeof(float));

            status = interpreter.Invoke();
            if (status == kTfLiteOk) {
                float* output_data = interpreter.output(0)->data.f;

                int pred_idx = 0;
                float max_val = output_data[0];
                for (int i = 1; i < MODEL_OUTPUT_CLASS_NUM; i++) {
                    if (output_data[i] > max_val) {
                        max_val = output_data[i];
                        pred_idx = i;
                    }
                }

                fprintf(stdout, "\r[%d] %s: %.4f   ", frame_count, class_labels[pred_idx], max_val);
                fflush(stdout);

                char text[128];
                snprintf(text, sizeof(text), "%s: %.2f", class_labels[pred_idx], max_val);
                putText(annotated, text, Point(10, 30), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
            }
        } else {
            fprintf(stdout, "\r[%d] No detection         ", frame_count);
            fflush(stdout);
        }

        draw_detection(annotated, detection);
        // imshow("SmartCar Detection", annotated);

        // if (waitKey(1) == 'q') {
        //     break;
        // }
    }

    fprintf(stdout, "\n");
    destroyAllWindows();
    munmap(model_buffer, model_size);
    free(arena_raw);
    _exit(0);
}