from __future__ import annotations

import io
import os
import re

import pytest

from PIL import features, Image, ImageCms
from Tests.helper import (
    assert_image_equal,
    assert_image_similar,
    assert_image_similar_tofile,
    hopper,
    skip_unless_feature,
)

try:
    from PIL import JpegXLImagePlugin
except ImportError:
    # Skipped via setup_module()
    pass


try:
    from defusedxml import ElementTree
except ImportError:
    ElementTree = None


class VerboseIO:
    # TEMP test class
    def __init__(self, wrap):
        self.fp = wrap

    def read(self, size=-1):
        print(f"reading {size} bytes, ", end="")
        data = self.fp.read(size)
        print(f"read {len(data)} bytes")
        return data

    def readline(self, size=-1):
        print(f"readline {size}")
        return self.fp.readline(size)

    def seek(self, offset, whence=os.SEEK_SET):
        print(f"seeking {offset} whence {whence}")
        return self.fp.seek(offset, whence)

    def tell(self):
        off = self.fp.tell()
        print(f"tell {off}")
        return off


def setup_module():
    try:
        from PIL import JpegXLImagePlugin

        # need to hit getattr to trigger the delayed import error
        assert JpegXLImagePlugin._jxl.JxlDecoder
    except ImportError as v:
        pytest.skip(str(v))


def test_sanity():
    assert re.search(r"\d+\.\d+\.\d+$", features.version_module("jpegxl"))


def test_simple():
    # cjxl Tests/images/hopper.png Tests/images/hopper.jxl
    with open("Tests/images/hopper.jxl", "rb") as f:
        data = f.read()
    with Image.open(VerboseIO(io.BytesIO(data))) as im:
        assert isinstance(im, JpegXLImagePlugin.JpegXLImageFile)
        assert im._type == "codestream"
        info = im._basic_info
        assert info["size"] == (128, 128)
        assert info["bits_per_sample"] == 8
        assert info["exponent_bits_per_sample"] == 0
        assert info["intensity_target"] == 255.0
        assert info["min_nits"] == 0.0
        assert info["relative_to_max_display"] is False
        assert info["linear_below"] == 0.0
        assert info["uses_original_profile"] is False
        assert info["preview_size"] is None
        assert info["animation_info"] is None
        assert info["orientation"] == 1
        assert info["num_color_channels"] == 3
        assert info["num_extra_channels"] == 0
        assert info["alpha_bits"] == 0
        assert info["alpha_exponent_bits"] == 0
        assert info["alpha_premultiplied"] == 0
        assert info["intrinsic_size"] == (128, 128)
        assert info["num_frames"] == 1
        assert info["box_types"] == []
        assert im.size == (128, 128)
        assert im.mode == "RGB"
        assert im.n_frames == 1
        assert "RGB_D65_SRG_Rel_g0".encode("utf-16le") in im.info["icc_profile"]
        im.load()
        assert im._frame_info["duration"] == 0
        assert im._frame_info["timecode"] == 0
        assert im._frame_info["name"] == b""
        assert im._frame_info["is_last"] is True
        assert len(im.getexif()) == 0
        if ElementTree is None:
            with pytest.warns(
                UserWarning,
                match="XMP data cannot be read without defusedxml dependency",
            ):
                assert im.getxmp() == {}
        else:
            assert im.getxmp() == {}
        assert_image_similar_tofile(im, "Tests/images/hopper.png", 8.25)
        assert_image_similar_tofile(im, "Tests/images/hopper.jpg", 8.3)


def test_lossless_jpeg():
    # cjxl Tests/images/hopper.jpg Tests/images/hopper.jpg.jxl
    with Image.open("Tests/images/hopper.jpg.jxl") as im:
        assert im._type == "container"
        assert im.size == (128, 128)
        assert im.mode == "RGB"
        assert im.n_frames == 1
        assert im._basic_info["box_types"] == [
            b"JXL ",
            b"ftyp",
            b"jxlp",
            b"jbrd",
            b"jxlp",
        ]
        assert_image_similar_tofile(im, "Tests/images/hopper.jpg", 1.8)


def test_exif_xmp():
    # cjxl Tests/images/pil_sample_rgb.jpg Tests/images/pil_sample_rgb.jxl
    with Image.open("Tests/images/pil_sample_rgb.jxl") as im:
        assert im._type == "container"
        assert im.size == (100, 100)
        assert im.mode == "RGB"
        assert im.n_frames == 1
        assert im._basic_info["box_types"] == [
            b"JXL ",
            b"ftyp",
            b"jxlp",
            b"jbrd",
            b"Exif",
            b"xml ",
            b"jxlp",
        ]
        assert_image_similar_tofile(im, "Tests/images/pil_sample_rgb.jpg", 1.2)

    info = im.getexif()
    assert info[305] == "Adobe Photoshop CS Macintosh"

    if ElementTree is None:
        with pytest.warns(
            UserWarning,
            match="XMP data cannot be read without defusedxml dependency",
        ):
            assert im.getxmp() == {}
    else:
        xmp = im.getxmp()

        description = xmp["xmpmeta"]["RDF"]["Description"]
        assert description[0]["about"] == "uuid:903dad8d-0c2e-11de-aa32-f3694bed6de0"


@skip_unless_feature("littlecms2")
def test_load_icc_profile():
    with Image.open("Tests/images/hopper.jxl") as im:
        pass
    profile = ImageCms.getOpenProfile(io.BytesIO(im.info["icc_profile"]))
    assert profile.profile.profile_description == "RGB_D65_SRG_Rel_g0.45455"


def test_animated():
    # cjxl Tests/images/apng/delay.png Tests/images/delay.jxl
    with Image.open("Tests/images/delay.jxl") as im:
        assert im._type == "codestream"
        assert im.n_frames == 5
        assert im.is_animated
        assert im.info["loop"] == 0
        with Image.open("Tests/images/apng/delay.png") as expected:
            im.load()
            expected.load()
            assert im._frame_info["duration"] == 100
            assert im._frame_info["is_last"] is False
            assert_image_similar(im, expected, 2.9)

            im.seek(1)
            im.load()
            expected.seek(1)
            assert im._frame_info["duration"] == expected.info["duration"]
            assert im._frame_info["is_last"] is False
            assert_image_similar(im, expected, 0.02)

            im.seek(2)
            im.load()
            expected.seek(2)
            assert im._frame_info["duration"] == expected.info["duration"]
            assert im._frame_info["is_last"] is False
            assert_image_similar(im, expected, 0.17)

            im.seek(3)
            im.load()
            expected.seek(3)
            assert im._frame_info["duration"] == expected.info["duration"]
            assert im._frame_info["is_last"] is False
            assert_image_similar(im, expected, 0.02)

            im.seek(4)
            im.load()
            expected.seek(4)
            assert im._frame_info["duration"] == expected.info["duration"]
            assert im._frame_info["is_last"] is True
            assert_image_similar(im, expected, 0.17)


@pytest.mark.xfail(reason="failing")
def test_num_plays():
    # cjxl apng/num_plays_1.png num_plays_1.jxl
    with Image.open("Tests/images/num_plays_1.jxl") as im:
        assert im.info["loop"] == 1


def test_close():
    im = Image.open("Tests/images/hopper.jxl")
    assert im.fp
    im.close()
    assert not im.fp
    with pytest.raises(Exception):
        im.load()


@pytest.mark.skip("unclear if we want this")
def test_exit():
    # TODO should image be closed on __exit__?
    with Image.open("Tests/images/hopper.jxl") as im:
        assert im.fp
    assert not im.fp
    with pytest.raises(Exception):
        im.load()


def test_boxes_max_count():
    with Image.open("Tests/images/pil_sample_rgb.jxl") as im:
        assert len(im._decoder.get_boxes("jxlp", max_count=-1)) == 2
        assert len(im._decoder.get_boxes("jxlp", max_count=0)) == 0
        assert len(im._decoder.get_boxes("jxlp", max_count=1)) == 1
        assert len(im._decoder.get_boxes("jxlp", max_count=2)) == 2
        assert len(im._decoder.get_boxes("jxlp", max_count=3)) == 2


@pytest.mark.parametrize("mode", ["L", "LA", "RGB", "RGBA", "I;16", "F"])
def test_modes(mode):
    suffix = mode.replace(";", "")
    prefix = f"Tests/images/hopper_{suffix}"

    tgt = hopper(mode)
    if "A" in mode:
        tgt.putalpha(hopper("L").transpose(Image.Transpose.ROTATE_90))
    if mode == "I;16":
        tgt = tgt.point(lambda val: val * 64)

    with Image.open(prefix + ".jxl") as im:
        assert im.mode == mode
        assert_image_equal(im, tgt)
