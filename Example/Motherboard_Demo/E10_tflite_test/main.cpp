#include "zf_common_headfile.h"
#include <dirent.h>
using namespace cv;

#define MODEL_INPUT_WIDTH         96
#define MODEL_INPUT_HEIGHT        96
#define MODEL_INPUT_CHANNEL       3
#define MODEL_INPUT_SIZE          (MODEL_INPUT_WIDTH * MODEL_INPUT_HEIGHT * MODEL_INPUT_CHANNEL)
#define MODEL_OUTPUT_CLASS_NUM    3
#define TFLITE_OP_RESOLVER_MAX_NUM 20
#define TENSOR_ARENA_SIZE         (256 * 1024)

const char* class_labels[] = {"交通工具-直行", "武器-左", "物资-右"};

int main(int, char**) {
    const char* model_path = "smartcar_model.tflite";
    const char* test_dir = "data/smartcar/test";

    uint8_t* model_buffer = (uint8_t*)malloc(5 * 1024 * 1024);
    if (!model_buffer) {
        printf("Failed to allocate model buffer\n");
        return -1;
    }

    FILE* fp = fopen(model_path, "rb");
    if (!fp) {
        printf("Failed to open model file: %s\n", model_path);
        free(model_buffer);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    size_t model_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(model_buffer, 1, model_size, fp);
    fclose(fp);
    printf("Model loaded: %zu bytes\n", model_size);

    tflite::InitializeTarget();

    const tflite::Model* model = ::tflite::GetModel(model_buffer);
    if (!model) {
        printf("Failed to get model from buffer\n");
        free(model_buffer);
        return -1;
    }
    printf("Model pointer: %p\n", (void*)model);
    printf("Model version: %d, expected: %d\n", model->version(), TFLITE_SCHEMA_VERSION);

    tflite::MicroMutableOpResolver<TFLITE_OP_RESOLVER_MAX_NUM> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddShape();
    resolver.AddRelu6();

    uint8_t tensor_arena[TENSOR_ARENA_SIZE];
    tflite::MicroInterpreter interpreter(model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("Failed to allocate tensors\n");
        free(model_buffer);
        return -1;
    }

    printf("Arena used: %zu bytes\n", interpreter.arena_used_bytes());

    int correct = 0;
    int total = 0;
    int class_counts[MODEL_OUTPUT_CLASS_NUM] = {0};
    int class_correct[MODEL_OUTPUT_CLASS_NUM] = {0};

    for (int class_idx = 0; class_idx < MODEL_OUTPUT_CLASS_NUM; class_idx++) {
        char class_dir[256];
        snprintf(class_dir, sizeof(class_dir), "%s/%s", test_dir, class_labels[class_idx]);

        DIR* dir = opendir(class_dir);
        if (!dir) continue;

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            char* ext = strrchr(entry->d_name, '.');
            if (!ext) continue;
            if (strcmp(ext, ".jpg") != 0 && strcmp(ext, ".png") != 0 && strcmp(ext, ".jpeg") != 0) continue;

            char img_path[512];
            snprintf(img_path, sizeof(img_path), "%s/%s", class_dir, entry->d_name);

            Mat src_img = imread(img_path, 1);
            if (src_img.empty()) continue;

            Mat resized_img;
            resize(src_img, resized_img, Size(MODEL_INPUT_WIDTH, MODEL_INPUT_HEIGHT), INTER_LINEAR);

            resized_img.convertTo(resized_img, CV_32FC3, 1.0f / 127.5f);
            subtract(resized_img, Scalar(1.0f, 1.0f, 1.0f), resized_img);

            cvtColor(resized_img, resized_img, cv::COLOR_BGR2RGB);

            Mat continuous_img = resized_img.isContinuous() ? resized_img : resized_img.clone();
            float* input_data = interpreter.input(0)->data.f;
            memcpy(input_data, continuous_img.ptr<float>(), MODEL_INPUT_SIZE * sizeof(float));

            if (interpreter.Invoke() != kTfLiteOk) {
                printf("Inference failed for %s\n", img_path);
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
            }

            if (strcmp(class_labels[pred_idx], class_labels[class_idx]) != 0) {
                printf("ERROR: %s -> predicted: %s, actual: %s\n",
                       entry->d_name, class_labels[pred_idx], class_labels[class_idx]);
            }
        }
        closedir(dir);
    }

    printf("\n========== Test Results ==========\n");
    for (int i = 0; i < MODEL_OUTPUT_CLASS_NUM; i++) {
        float acc = class_counts[i] > 0 ? (100.0f * class_correct[i] / class_counts[i]) : 0;
        printf("%s: %d/%d (%.1f%%)\n", class_labels[i], class_correct[i], class_counts[i], acc);
    }
    printf("---------------------------------\n");
    printf("Total: %d/%d (%.1f%%)\n", correct, total, 100.0f * correct / total);
    printf("==================================\n");

    free(model_buffer);
    return 0;
}