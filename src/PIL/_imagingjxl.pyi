from __future__ import annotations

from typing import TypedDict

class AnimationInfo(TypedDict):
    tps: tuple[int, int]
    num_loops: int
    have_timecodes: bool

class BasicInfo(TypedDict):
    have_container: bool
    size: tuple[int, int]
    bits_per_sample: int
    exponent_bits_per_sample: int
    intensity_target: float
    min_nits: float
    relative_to_max_display: bool
    linear_below: float
    uses_original_profile: bool
    preview_size: tuple[int, int] | None
    animation_info: AnimationInfo | None
    orientation: int
    num_color_channels: int
    num_extra_channels: int
    alpha_bits: int
    alpha_exponent_bits: int
    alpha_premultiplied: bool
    intrinsic_size: tuple[int, int]
    num_frames: int
    box_types: list[bytes]

class FrameInfo(TypedDict):
    duration: int
    timecode: int
    name: bytes
    is_last: bool

class JxlDecoder:
    def __new__(cls, data: bytes) -> JxlDecoder: ...  # data may be bytes-like object
    def get_info(self) -> BasicInfo: ...
    def get_boxes(
        self, type: bytes, max_count: int = -1
    ) -> list[bytes]: ...  # TODO max size?
    def get_icc_profile(self) -> bytes | None: ...
    def next(self, im_id: int, /) -> FrameInfo | None: ...
    def skip(self, frames: int, /) -> None: ...
    def rewind(self) -> None: ...
    @property
    def frame_no(self) -> int: ...

def check_signature(data: bytes, /) -> str | None: ...
