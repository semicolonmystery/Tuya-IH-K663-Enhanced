#!/usr/bin/env python3
"""
make_ota_index.py — produce a Zigbee2MQTT OTA index entry for an .ota image.

Parses the standard Zigbee OTA header (written by make_ota.py) to recover the
manufacturer code / image type / file version, hashes the file, and emits a
one-element JSON index array that Z2M can consume via an index override.

  make_ota_index.py <in.ota> --url <download-url> \
      --model-id TS0041-Enhanced --manufacturer-name _TZ3000_fa9mlvja-Enhanced \
      [--out index.json]
"""
import argparse
import hashlib
import json
import struct

OTA_HDR = struct.Struct("<I5HIH32sI")  # matches tools/make_ota.py


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("ota")
    p.add_argument("--url", required=True)
    p.add_argument("--model-id", required=True)
    p.add_argument("--manufacturer-name", required=True)
    p.add_argument("--out")
    a = p.parse_args()

    with open(a.ota, "rb") as f:
        data = f.read()

    (_magic, _hver, _hlen, _fctl, manuf, image_type, file_version,
     _zbver, _hstr, total_size) = OTA_HDR.unpack(data[:OTA_HDR.size])

    entry = {
        "fileVersion": file_version,
        "fileSize": total_size,
        "manufacturerCode": manuf,
        "imageType": image_type,
        "sha512": hashlib.sha512(data).hexdigest(),
        "url": a.url,
        "modelId": a.model_id,
        "manufacturerName": [a.manufacturer_name],
    }
    out = json.dumps([entry], indent=2)
    if a.out:
        with open(a.out, "w") as f:
            f.write(out + "\n")
        print(f"wrote {a.out}")
    else:
        print(out)


if __name__ == "__main__":
    main()
