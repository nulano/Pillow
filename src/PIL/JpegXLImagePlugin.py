from __future__ import annotations

from . import Image, ImageFile
from ._util import DeferredError

try:
    from . import _imagingjxl as _jxl
except ImportError as ex:
    _jxl = DeferredError.new(ex)


def _accept(prefix):
    return (
        prefix[:2] == b"\xFF\x0A"
        or prefix[:12] == b"\0\0\0\x0C\x4A\x58\x4C\x20\x0D\x0A\x87\x0A"
    )


class JpegXLImageFile(ImageFile.ImageFile):
    format = "JPEGXL"
    format_description = "JPEG XL (ISO/IEC 18181)"

    def _open(self):
        # TODO when to close self.fp?
        self._decoder = _jxl.JxlDecoder(self.fp)

        self._basic_info = self._decoder.get_info()

        width, height = self._basic_info["size"]
        if 5 <= self._basic_info["orientation"] <= 8:
            width, height = height, width
        self._size = (width, height)

        have_alpha = self._basic_info["alpha_bits"] != 0
        if self._basic_info["num_color_channels"] == 1:
            # TODO check bit depth (image may be "I")
            self._mode = "LA" if have_alpha else "L"
        elif self._basic_info["num_color_channels"] == 3:
            self._mode = "RGBA" if have_alpha else "RGB"
        else:
            msg = "cannot determine image mode"
            raise SyntaxError(msg)

        self.is_animated = self._basic_info["animation_info"] is not None
        self.tile = []

        icc_profile = self._decoder.get_icc_profile()
        if icc_profile:
            self.info["icc_profile"] = icc_profile

    # def load(self):


Image.register_open(JpegXLImageFile.format, JpegXLImageFile, _accept)

Image.register_extensions(JpegXLImageFile.format, [".jxl"])
