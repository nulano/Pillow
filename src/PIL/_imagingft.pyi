from __future__ import annotations

from typing import Any, Callable, Sequence, TypedDict

HAVE_RAQM: bool
HAVE_FRIBIDI: bool
HAVE_HARFBUZZ: bool

freetype2_version: str | None
raqm_version: str | None
fribidi_version: str | None
harfbuzz_version: str | None

class FontVariationAxis(TypedDict):
    minimum: int | None
    default: int | None
    maximum: int | None
    name: bytes | None

class Font:
    def render(
        self,
        string: str,
        fill: Callable[[int, int], Any],  # TODO returns internal image type
        mode: str | None = None,
        dir: str | None = None,
        features: Sequence[str] | None = None,
        lang: str | None = None,
        stroke_width: int = 0,
        anchor: str | None = None,
        foreground_ink_long: int = 0,
        x_start: float = 0,
        y_start: float = 0,
    ) -> tuple[Any, tuple[int, int]]: ...  # TODO returns internal image type
    def getsize(
        self,
        string: str,
        mode: str | None = None,
        dir: str | None = None,
        features: Sequence[str] | None = None,
        lang: str | None = None,
        anchor: str | None = None,
    ) -> tuple[tuple[int, int], tuple[int, int]]: ...
    def getlength(
        self,
        string: str,
        mode: str | None = None,
        dir: str | None = None,
        features: Sequence[str] | None = None,
        lang: str | None = None,
    ) -> int: ...

    if ...:  # freetype2_version >= 2.9.1
        def getvarnames(self) -> list[bytes]: ...
        def getvaraxes(self) -> list[FontVariationAxis]: ...
        def setvarname(self, index: int) -> None: ...
        def setvaraxes(self, axes: list[float]) -> None: ...

    @property
    def family(self) -> str: ...
    @property
    def style(self) -> str: ...
    @property
    def ascent(self) -> int: ...
    @property
    def descent(self) -> int: ...
    @property
    def height(self) -> int: ...
    @property
    def x_ppem(self) -> int: ...
    @property
    def y_ppem(self) -> int: ...
    @property
    def glyphs(self) -> int: ...

def getfont(
    filename: str | bytes | bytearray,
    size: float,
    index: int = 0,
    encoding: str = "",
    font_bytes: bytes | bytearray = b"",
    layout_engine: int = 0,
) -> Font: ...
