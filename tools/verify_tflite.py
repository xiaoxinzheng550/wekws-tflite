#!/usr/bin/env python3
"""Load a TFLite model, print its tensors, and run one zero-input inference."""

import argparse
from pathlib import Path

import numpy as np
import tensorflow as tf


def quantized_zero(detail):
    scale, zero_point = detail["quantization"]
    fill_value = zero_point if scale else 0
    return np.full(detail["shape"], fill_value, dtype=detail["dtype"])


def main():
    project_dir = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "model",
        nargs="?",
        type=Path,
        default=(project_dir / "model" / "tflite"
                 / "ds_tcn_fixed_quantized_backup.tflite"),
    )
    args = parser.parse_args()

    interpreter = tf.lite.Interpreter(model_path=str(args.model))
    interpreter.allocate_tensors()
    inputs = interpreter.get_input_details()
    outputs = interpreter.get_output_details()

    print(f"model: {args.model}")
    for index, detail in enumerate(inputs):
        print(
            f"input[{index}] name={detail['name']} shape={detail['shape'].tolist()} "
            f"dtype={detail['dtype'].__name__} quantization={detail['quantization']}"
        )
        interpreter.set_tensor(detail["index"], quantized_zero(detail))

    interpreter.invoke()
    for index, detail in enumerate(outputs):
        value = interpreter.get_tensor(detail["index"])
        print(
            f"output[{index}] name={detail['name']} shape={value.shape} "
            f"dtype={value.dtype} range=[{value.min()}, {value.max()}]"
        )


if __name__ == "__main__":
    main()
