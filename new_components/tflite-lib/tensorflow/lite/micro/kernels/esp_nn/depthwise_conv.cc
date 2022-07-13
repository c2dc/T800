/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/micro/kernels/depthwise_conv.h"

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/kernels/internal/common.h"
#include "tensorflow/lite/kernels/internal/quantization_util.h"
#include "tensorflow/lite/kernels/internal/reference/depthwiseconv_float.h"
#include "tensorflow/lite/kernels/internal/reference/depthwiseconv_uint8.h"
#include "tensorflow/lite/kernels/internal/reference/integer_ops/depthwise_conv.h"
#include "tensorflow/lite/kernels/internal/tensor_ctypes.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/kernels/padding.h"
#include "tensorflow/lite/micro/kernels/kernel_util.h"

#include "freertos/FreeRTOS.h"
#include <esp_timer.h>

#if ESP_NN
#include <esp_nn.h>
#endif

long long dc_total_time = 0;

namespace tflite {
namespace {
#if ESP_NN
static int buffer_idx = -1;
#endif
void* Init(TfLiteContext* context, const char* buffer, size_t length) {
  TFLITE_DCHECK(context->AllocatePersistentBuffer != nullptr);
  return context->AllocatePersistentBuffer(context, sizeof(OpDataConv));
}

#if ESP_NN
inline void EvalQuantizedPerChannel(TfLiteContext* context, TfLiteNode* node,
                                    const TfLiteDepthwiseConvParams& params,
                                    const OpDataConv& data,
                                    const TfLiteEvalTensor* input,
                                    const TfLiteEvalTensor* filter,
                                    const TfLiteEvalTensor* bias,
                                    TfLiteEvalTensor* output) {
  const int dilation_width_factor = params.dilation_width_factor;
  const int dilation_height_factor = params.dilation_height_factor;

  if (dilation_width_factor == 1 && dilation_height_factor == 1) {
    // Get parameters.
    RuntimeShape input_shape = tflite::micro::GetTensorShape(input);
    RuntimeShape filter_shape = tflite::micro::GetTensorShape(filter);
    RuntimeShape output_shape = tflite::micro::GetTensorShape(output);
    RuntimeShape bias_shape = tflite::micro::GetTensorShape(bias);

    TFLITE_DCHECK_EQ(input_shape.DimensionsCount(), 4);
    TFLITE_DCHECK_EQ(filter_shape.DimensionsCount(), 4);
    TFLITE_DCHECK_EQ(output_shape.DimensionsCount(), 4);

    const int8_t *input_data = tflite::micro::GetTensorData<int8_t>(input);
    int8_t *output_data = tflite::micro::GetTensorData<int8_t>(output);

    const int depth_multiplier = params.depth_multiplier;
    const int32_t input_offset = -data.input_zero_point;
    const int32_t output_offset = data.output_zero_point;
    const int stride_width = params.stride_width;
    const int stride_height = params.stride_height;
    const int pad_width = data.padding.width;
    const int pad_height = data.padding.height;

    const int input_height = input_shape.Dims(1);
    const int input_width = input_shape.Dims(2);
    const int input_depth = input_shape.Dims(3);
    const int filter_height = filter_shape.Dims(1);
    const int filter_width = filter_shape.Dims(2);
    const int output_height = output_shape.Dims(1);
    const int output_width = output_shape.Dims(2);

    // Set min and max value of the output.
    const int32_t activation_min = data.output_activation_min;
    const int32_t activation_max = data.output_activation_max;

    // Consistency check.
    TFLITE_DCHECK_LE(activation_min, activation_max);
    const int batch_size = MatchingDim(input_shape, 0, output_shape, 0);
    const int output_depth = MatchingDim(filter_shape, 3, output_shape, 3);

    TFLITE_DCHECK_EQ(output_depth, input_depth * depth_multiplier);
    if (tflite::micro::GetTensorData<int8_t>(bias)) {
      TFLITE_DCHECK_EQ(bias_shape.FlatSize(), output_depth);
    }

    const int input_size = input_width * input_height * input_depth;
    const int output_size = output_width * output_height * output_depth;
    void *scratch_buf = NULL;// = calloc(1, scratch_buf_size + 128 + 16);
    if (buffer_idx > -1) {
      scratch_buf = context->GetScratchBuffer(context, buffer_idx);
    }
    esp_nn_set_depthwise_conv_scratch_buf(scratch_buf);

    for (int i_batch = 0; i_batch < batch_size; i_batch++) {
      esp_nn_depthwise_conv_s8(input_data + i_batch * input_size, input_width,
                               input_height, input_depth, input_offset,
                               pad_width, pad_height,
                               stride_width, stride_height, depth_multiplier,
                               tflite::micro::GetTensorData<int8_t>(filter),
                               filter_width, filter_height,
                               tflite::micro::GetTensorData<int32_t>(bias),
                               output_data + i_batch * output_size,
                               output_width, output_height, output_offset,
                               data.per_channel_output_shift,
                               data.per_channel_output_multiplier,
                               activation_min, activation_max);
    }
  } else {
    reference_integer_ops::DepthwiseConvPerChannel(
        DepthwiseConvParamsQuantized(params, data),
        data.per_channel_output_multiplier, data.per_channel_output_shift,
        tflite::micro::GetTensorShape(input),
        tflite::micro::GetTensorData<int8_t>(input),
        tflite::micro::GetTensorShape(filter),
        tflite::micro::GetTensorData<int8_t>(filter),
        tflite::micro::GetTensorShape(bias),
        tflite::micro::GetTensorData<int32_t>(bias),
        tflite::micro::GetTensorShape(output),
        tflite::micro::GetTensorData<int8_t>(output));
  }
}
#endif

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TFLITE_DCHECK(node->user_data != nullptr);
  TFLITE_DCHECK(node->builtin_data != nullptr);

  OpDataConv* data = static_cast<OpDataConv*>(node->user_data);
  const TfLiteDepthwiseConvParams& params =
      *(static_cast<const TfLiteDepthwiseConvParams*>(node->builtin_data));

  TfLiteTensor* output = GetOutput(context, node, kDepthwiseConvOutputTensor);
  TF_LITE_ENSURE(context, output != nullptr);
  const TfLiteTensor* input =
      GetInput(context, node, kDepthwiseConvInputTensor);
  TF_LITE_ENSURE(context, input != nullptr);
  const TfLiteTensor* filter =
      GetInput(context, node, kDepthwiseConvWeightsTensor);
  TF_LITE_ENSURE(context, filter != nullptr);

  const int input_width = input->dims->data[2];
  const int input_height = input->dims->data[1];
  const int filter_width = filter->dims->data[2];
  const int filter_height = filter->dims->data[1];
  const int output_width = output->dims->data[2];
  const int output_height = output->dims->data[1];

  // Dynamically allocate per-channel quantization parameters.
  const int num_channels = filter->dims->data[kDepthwiseConvQuantizedDimension];
  data->per_channel_output_multiplier =
      static_cast<int32_t*>(context->AllocatePersistentBuffer(
          context, num_channels * sizeof(int32_t)));
  data->per_channel_output_shift =
      static_cast<int32_t*>(context->AllocatePersistentBuffer(
          context, num_channels * sizeof(int32_t)));

  // All per-channel quantized tensors need valid zero point and scale arrays.
  if (input->type == kTfLiteInt8) {
    TF_LITE_ENSURE_EQ(context, filter->quantization.type,
                      kTfLiteAffineQuantization);

    const auto* affine_quantization =
        static_cast<TfLiteAffineQuantization*>(filter->quantization.params);
    TFLITE_DCHECK(affine_quantization != nullptr);
    TFLITE_DCHECK(affine_quantization->scale != nullptr);
    TFLITE_DCHECK(affine_quantization->zero_point != nullptr);

    TF_LITE_ENSURE(
        context, affine_quantization->scale->size == 1 ||
                     affine_quantization->scale->size ==
                         filter->dims->data[kDepthwiseConvQuantizedDimension]);

    TF_LITE_ENSURE_EQ(context, affine_quantization->scale->size,
                      affine_quantization->zero_point->size);
  }

  TF_LITE_ENSURE_STATUS(CalculateOpDataDepthwiseConv(
      context, node, params, input_width, input_height, filter_width,
      filter_height, output_width, output_height, input->type, data));

#if ESP_NN
  if (input->type == kTfLiteInt8) {
    int scratch_buf_size = esp_nn_get_depthwise_conv_scratch_size(
        input_width, input_height, input->dims->data[3],
        params.depth_multiplier, filter_width, filter_height);
    if (scratch_buf_size > 0) {
      TF_LITE_ENSURE_STATUS(context->RequestScratchBufferInArena(
        context, scratch_buf_size, &buffer_idx));
    }
  }
#endif
  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  TFLITE_DCHECK(node->user_data != nullptr);
  TFLITE_DCHECK(node->builtin_data != nullptr);

  auto& params =
      *(reinterpret_cast<TfLiteDepthwiseConvParams*>(node->builtin_data));
  const OpDataConv& data = *(static_cast<const OpDataConv*>(node->user_data));

  TfLiteEvalTensor* output =
      tflite::micro::GetEvalOutput(context, node, kDepthwiseConvOutputTensor);
  const TfLiteEvalTensor* input =
      tflite::micro::GetEvalInput(context, node, kDepthwiseConvInputTensor);
  const TfLiteEvalTensor* filter =
      tflite::micro::GetEvalInput(context, node, kDepthwiseConvWeightsTensor);
  const TfLiteEvalTensor* bias =
      (NumInputs(node) == 3)
          ? tflite::micro::GetEvalInput(context, node, kDepthwiseConvBiasTensor)
          : nullptr;

  long long start_time = esp_timer_get_time();
  switch (input->type) {  // Already know in/out types are same.
    case kTfLiteFloat32:
      tflite::reference_ops::DepthwiseConv(
          DepthwiseConvParamsFloat(params, data),
          tflite::micro::GetTensorShape(input),
          tflite::micro::GetTensorData<float>(input),
          tflite::micro::GetTensorShape(filter),
          tflite::micro::GetTensorData<float>(filter),
          tflite::micro::GetTensorShape(bias),
          tflite::micro::GetTensorData<float>(bias),
          tflite::micro::GetTensorShape(output),
          tflite::micro::GetTensorData<float>(output));
      break;
    case kTfLiteInt8:
#if ESP_NN
      EvalQuantizedPerChannel(context, node, params, data, input, filter, bias,
                              output);
#else
      reference_integer_ops::DepthwiseConvPerChannel(
          DepthwiseConvParamsQuantized(params, data),
          data.per_channel_output_multiplier, data.per_channel_output_shift,
          tflite::micro::GetTensorShape(input),
          tflite::micro::GetTensorData<int8_t>(input),
          tflite::micro::GetTensorShape(filter),
          tflite::micro::GetTensorData<int8_t>(filter),
          tflite::micro::GetTensorShape(bias),
          tflite::micro::GetTensorData<int32_t>(bias),
          tflite::micro::GetTensorShape(output),
          tflite::micro::GetTensorData<int8_t>(output));
#endif
      break;
    case kTfLiteUInt8:
      //EvalQuantized(context, node, params, &data, input, filter, bias, output);
      reference_ops::DepthwiseConv(
          DepthwiseConvParamsQuantized(params, data),
          tflite::micro::GetTensorShape(input),
          tflite::micro::GetTensorData<uint8_t>(input),
          tflite::micro::GetTensorShape(filter),
          tflite::micro::GetTensorData<uint8_t>(filter),
          tflite::micro::GetTensorShape(bias),
          tflite::micro::GetTensorData<int32_t>(bias),
          tflite::micro::GetTensorShape(output),
          tflite::micro::GetTensorData<uint8_t>(output));
      break;
    default:
      TF_LITE_KERNEL_LOG(context, "Type %s (%d) not supported.",
                         TfLiteTypeGetName(input->type), input->type);
      return kTfLiteError;
  }
  dc_total_time += esp_timer_get_time() - start_time;
  return kTfLiteOk;
}

}  // namespace

TfLiteRegistration Register_DEPTHWISE_CONV_2D() {
  return {/*init=*/Init,
          /*free=*/nullptr,
          /*prepare=*/Prepare,
          /*invoke=*/Eval,
          /*profiling_string=*/nullptr,
          /*builtin_code=*/0,
          /*custom_name=*/nullptr,
          /*version=*/0};
}

}  // namespace tflite
