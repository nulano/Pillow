from __future__ import annotations

from typing import BinaryIO, TypedDict

class AnimationInfo(TypedDict):
    tps: tuple[int, int]
    num_loops: int
    have_timecodes: bool

class BasicInfo(TypedDict):
    size: tuple[int, int]
    bits_per_sample: int
    uses_original_profile: bool
    preview_size: tuple[int, int] | None
    animation_info: AnimationInfo | None
    orientation: int
    num_color_channels: int
    num_extra_channels: int
    alpha_bits: int

class JxlDecoder:
    def __new__(cls, fp: BinaryIO) -> JxlDecoder: ...
    def get_info(self) -> BasicInfo: ...
    def get_icc_profile(self) -> bytes | None: ...
