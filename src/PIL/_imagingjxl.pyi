from typing import BinaryIO, TypedDict

class BasicInfo(TypedDict):
    size: tuple[int, int]
    bits_per_sample: int
    uses_original_profile: bool
    have_preview: bool
    have_animation: bool
    orientation: int
    num_color_channels: int
    num_extra_channels: int
    alpha_bits: int

class JxlDecoder:
    def __new__(cls, fp: BinaryIO) -> JxlDecoder: ...
    def get_info(self) -> BasicInfo: ...
