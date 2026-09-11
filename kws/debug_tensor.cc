// 额外的调试代码，添加到 keyword_spotting.cc 中
// 在 AllocateTensors() 后调用此函数

#include "tensorflow/lite/schema/schema_generated.h"

void DebugPrintModelTensors(const tflite::Model* model) {
    auto subgraphs = model->subgraphs();
    if (subgraphs && subgraphs->size() > 0) {
        auto graph = subgraphs->Get(0);
        auto tensors = graph->tensors();
        auto inputs = graph->inputs();
        auto outputs = graph->outputs();

        std::cout << "\n=== FlatBuffer Model Direct Parse ===" << std::endl;
        std::cout << "Inputs count: " << inputs->size() << std::endl;
        std::cout << "Outputs count: " << outputs->size() << std::endl;

        for (int i = 0; i < inputs->size(); i++) {
            int tensor_idx = inputs->Get(i);
            auto tensor = tensors->Get(tensor_idx);
            std::cout << "Input[" << i << "] tensor_idx=" << tensor_idx
                      << " name=" << (tensor->name() ? tensor->name()->c_str() : "null")
                      << " FlatBuffer_type=" << static_cast<int>(tensor->type())
                      << " (FLOAT32=0)" << std::endl;
        }

        for (int i = 0; i < outputs->size(); i++) {
            int tensor_idx = outputs->Get(i);
            auto tensor = tensors->Get(tensor_idx);
            std::cout << "Output[" << i << "] tensor_idx=" << tensor_idx
                      << " name=" << (tensor->name() ? tensor->name()->c_str() : "null")
                      << " FlatBuffer_type=" << static_cast<int>(tensor->type())
                      << " (FLOAT32=0)" << std::endl;
        }
    }
}
