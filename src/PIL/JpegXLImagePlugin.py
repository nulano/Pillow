from __future__ import annotations

from . import Image, ImageFile
from ._util import DeferredError

try:
    from . import _imagingjxl as _jxl
except ImportError as ex:
    _jxl = DeferredError.new(ex)


def _accept(prefix):
    return _jxl.check_signature(prefix) is not None


class JpegXLImageFile(ImageFile.ImageFile):
    format = "JPEGXL"
    format_description = "JPEG XL (ISO/IEC 18181)"

    def _open(self) -> None:
        self.map = None
        if self.filename:
            try:
                # use mmap, if possible
                import mmap

                with open(self.filename) as fp:
                    self.map = mmap.mmap(fp.fileno(), 0, access=mmap.ACCESS_READ)
            except (AttributeError, OSError, ImportError):
                pass

        data = self.map if self.map is not None else self.fp.read()
        self._decoder = _jxl.JxlDecoder(data)

        self._type = _jxl.check_signature(data[:16])  # TODO rename field

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

        self._frame_info = None
        self._frame_current = 0
        self._frame_loaded = -1

    def _seek(self, frame):
        if frame < self._decoder.frame_no:
            self._decoder.rewind()
        frame -= self._decoder.frame_no
        if frame > 0:
            self._decoder.skip(frame)
        # elif frame < 0: raise ValueError(...)

    def load(self):
        if self.im is None:
            self.im = Image.core.fill(self._mode, self._size, 0)
        if self._frame_loaded != self._frame_current:
            self._seek(self._frame_current)
            self._frame_info = self._decoder.next(self.im.id)
            self._frame_loaded = self._frame_current
        return super().load()

    def tell(self) -> int:
        return self._frame_current

    def seek(self, frame):
        if frame < 0:
            return
        self._frame_current = frame


Image.register_open(JpegXLImageFile.format, JpegXLImageFile, _accept)

Image.register_extensions(JpegXLImageFile.format, [".jxl"])
