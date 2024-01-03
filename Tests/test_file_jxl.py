from __future__ import annotations

import io
import os
import re

import pytest

from PIL import features, Image

try:
    from PIL import JpegXLImagePlugin
except ImportError:
    # Skipped via setup_module()
    pass


class VerboseIO:
    # TEMP test class
    def __init__(self, wrap):
        self.fp = wrap

    def read(self, size):
        print(f"reading {size} bytes, ", end="")
        data = self.fp.read(size)
        print(f"read {len(data)} bytes")
        return data

    def seek(self, offset, whence=os.SEEK_SET):
        print(f"seeking {offset} whence {whence}")
        return self.fp.seek(offset, whence)


def setup_module():
    try:
        from PIL import JpegXLImagePlugin

        # need to hit getattr to trigger the delayed import error; TODO this is not true yet
        assert JpegXLImagePlugin._jxl.JxlDecoder
    except ImportError as v:
        pytest.skip(str(v))


def test_sanity():
    assert re.search(r"\d+\.\d+\.\d+$", features.version_module("jpegxl"))


def test_info():
    # TODO access via image plugin
    with open("images/hopper.jxl", "rb") as f:
        data = f.read()
    fp = VerboseIO(io.BytesIO(data))
    info = JpegXLImagePlugin._jxl.JxlDecoder(fp).get_info()
    assert info["size"] == (128, 128)
    assert info["bits_per_sample"] == 8
    assert info["uses_original_profile"] is False
    assert info["have_preview"] is False
    assert info["have_animation"] is False
    assert info["orientation"] == 1
    assert info["num_color_channels"] == 3
    assert info["num_extra_channels"] == 0
    assert info["alpha_bits"] == 0
