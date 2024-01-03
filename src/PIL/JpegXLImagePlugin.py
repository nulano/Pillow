from __future__ import annotations

from . import Image
from . import ImageFile
from . import _imagingjxl as _jxl


def _accept(prefix):
    return (
        prefix[:4] == b"\xFF\x0A"
        or prefix[:12] == b"\0\0\0\x0C\x4A\x58\x4C\x20\x0D\x0A\x87\x0A"
    )


class JpegXLImageFile(ImageFile.ImageFile):
    format = "JPEGXL"
    format_description = "JPEG XL (ISO/IEC 18181)"

    def _open(self):
        # TODO when to close self.fp?
        self._decoder = _jxl.JxlDecoder(self.fp)

        self._basic_info = self._decoder.get_info()

        self._size = self._basic_info["size"]

        self.tile = []


Image.register_open(JpegXLImageFile.format, JpegXLImageFile, _accept)

Image.register_extensions(JpegXLImageFile.format, [".jxl"])
