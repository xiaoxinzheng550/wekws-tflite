#!/usr/bin/env python3
"""Convert a TFLite flatbuffer into the model_data.cc embedded byte array."""

import argparse
from pathlib import Path


def main():
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    parser = argparse.ArgumentParser(description="TFLite -> model_data.cc")
    parser.add_argument(
        "input_tflite",
        nargs="?",
        type=Path,
        default=(project_dir / "model" / "tflite"
                 / "ds_tcn_fixed_quantized_backup.tflite"),
    )
    parser.add_argument(
        "output_cc",
        nargs="?",
        type=Path,
        default=project_dir / "model" / "model_data.cc",
    )
    args = parser.parse_args()

    data = args.input_tflite.read_bytes()
    hex_lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        hex_lines.append("  " + ", ".join(f"0x{byte:02x}" for byte in chunk))

    content = '#include "model/model_data.h"\n\n'
    content += "const unsigned char g_model_data[] = {\n"
    content += ",\n".join(hex_lines) + "\n};\n"
    content += f"const unsigned int g_model_data_len = {len(data)};\n"

    args.output_cc.parent.mkdir(parents=True, exist_ok=True)
    args.output_cc.write_text(content, encoding="utf-8")
    print(
        f"Successfully converted {args.input_tflite} ({len(data)} bytes) "
        f"to {args.output_cc}"
    )


if __name__ == "__main__":
    main()
