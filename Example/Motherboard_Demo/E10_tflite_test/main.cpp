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

const char* class_labels[] = {"class0", "class1", "class2"};
const char* class_dirs[] = {"class0", "class1", "class2"};

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

static Mat preprocess_image(const char* img_path, bool is_warped) {
    (void)is_warped;
    Mat src_img = imread(img_path, 1);
    if (src_img.empty()) {
        return Mat();
    }

    Mat img = src_img;
    img = resize_with_lanczos(img, MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT);

    img.convertTo(img, CV_32FC3, 1.0f / 127.5f);
    subtract(img, Scalar(1.0f, 1.0f, 1.0f), img);

    cvtColor(img, img, cv::COLOR_BGR2RGB);

    return img;
}

int main(int, char**) {
    fprintf(stderr, "E10 TFLite Test Program\n");
    fprintf(stderr, "========================\n");

    const char* model_path = "smartcar_model.tflite";
    const char* test_dir = "test_warped";
    bool use_warped_prefix = true;

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
        fprintf(stderr, "Note: This model may be too large or complex for TFLM\n");
        munmap(model_buffer, model_size);
        free(arena_raw);
        return -1;
    }

    fprintf(stderr, "Tensor allocation successful, arena used: %zu bytes\n", interpreter.arena_used_bytes());

    int correct = 0;
    int total = 0;
    int class_counts[MODEL_OUTPUT_CLASS_NUM] = {0};
    int class_correct[MODEL_OUTPUT_CLASS_NUM] = {0};

    for (int class_idx = 0; class_idx < MODEL_OUTPUT_CLASS_NUM; class_idx++) {
        char class_dir[256];
        snprintf(class_dir, sizeof(class_dir), "%s/%s", test_dir, class_dirs[class_idx]);

        DIR* dir = opendir(class_dir);
        if (!dir) {
            fprintf(stderr, "Warning: Cannot open directory %s\n", class_dir);
            continue;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            char* ext = strrchr(entry->d_name, '.');
            if (!ext) continue;
            if (strcmp(ext, ".jpg") != 0 && strcmp(ext, ".png") != 0 && strcmp(ext, ".jpeg") != 0) continue;

            bool is_warped = (strncmp(entry->d_name, "warped_", 8) == 0);

            char img_path[512];
            snprintf(img_path, sizeof(img_path), "%s/%s", class_dir, entry->d_name);

            Mat resized_img = preprocess_image(img_path, is_warped);
            if (resized_img.empty()) {
                fprintf(stderr, "Warning: Cannot read image %s\n", img_path);
                continue;
            }

            Mat continuous_img = resized_img.isContinuous() ? resized_img : resized_img.clone();
            float* input_data = interpreter.input(0)->data.f;
            memcpy(input_data, continuous_img.ptr<float>(), MODEL_INPUT_SIZE * sizeof(float));

            status = interpreter.Invoke();
            if (status != kTfLiteOk) {
                fprintf(stderr, "ERROR: Inference failed for %s\n", img_path);
                continue;
            }

            float* output_data = interpreter.output(0)->data.f;

            int pred_idx = 0;
            float max_val = output_data[0];
            for (int i = 1; i < MODEL_OUTPUT_CLASS_NUM; i++) {
                if (output_data[i] > max_val) {
                    max_val = output_data[i];
                    pred_idx = i;
                }
            }

            class_counts[class_idx]++;
            total++;
            if (pred_idx == class_idx) {
                correct++;
                class_correct[class_idx]++;
            } else {
                fprintf(stderr, "ERROR: %s -> predicted: %s, actual: %s\n",
                       entry->d_name, class_labels[pred_idx], class_labels[class_idx]);
            }
        }
        closedir(dir);
    }

    fprintf(stderr, "\n========== Test Results ==========\n");
    for (int i = 0; i < MODEL_OUTPUT_CLASS_NUM; i++) {
        float acc = class_counts[i] > 0 ? (100.0f * class_correct[i] / class_counts[i]) : 0;
        fprintf(stderr, "%s: %d/%d (%.1f%%)\n", class_labels[i], class_correct[i], class_counts[i], acc);
    }
    fprintf(stderr, "---------------------------------\n");
    fprintf(stderr, "Total: %d/%d (%.1f%%)\n", correct, total, total > 0 ? (100.0f * correct / total) : 0);
    fprintf(stderr, "==================================\n");

    munmap(model_buffer, model_size);
    free(arena_raw);
    _exit(0);
}