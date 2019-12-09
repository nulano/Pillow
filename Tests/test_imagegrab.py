import subprocess
import sys

from PIL import Image, ImageGrab

from .helper import PillowTestCase, unittest


class TestImageGrab(PillowTestCase):
    @unittest.skipUnless(
        sys.platform in ("win32", "darwin"), "requires Windows or macOS"
    )
    def test_grab(self):
        for im in [
            ImageGrab.grab(),
            ImageGrab.grab(include_layered_windows=True),
            ImageGrab.grab(all_screens=True),
        ]:
            self.assert_image(im, im.mode, im.size)

    @unittest.skipUnless(Image.core.HAVE_XCB, "requires XCB")
    def test_grab_x11(self):
        try:
            if sys.platform not in ("win32", "darwin"):
                im = ImageGrab.grab()
                self.assert_image(im, im.mode, im.size)

            im2 = ImageGrab.grab(xdisplay="")
            self.assert_image(im2, im2.mode, im2.size)
        except IOError as e:
            self.skipTest(str(e))

    @unittest.skipUnless(Image.core.HAVE_XCB, "requires XCB")
    def test_grab_invalid_xdisplay(self):
        exception = None

        try:
            ImageGrab.grab(xdisplay="error.test:0.0")
        except Exception as e:
            exception = e

        self.assertIsInstance(exception, IOError)
        self.assertTrue(str(exception).startswith("X connection failed"))

    def test_grabclipboard(self):
        if sys.platform == "darwin":
            subprocess.call(["screencapture", "-cx"])
        elif sys.platform == "win32":
            p = subprocess.Popen(["powershell", "-command", "-"], stdin=subprocess.PIPE)
            p.stdin.write(
                b"""[Reflection.Assembly]::LoadWithPartialName("System.Drawing")
[Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms")
$bmp = New-Object Drawing.Bitmap 200, 200
[Windows.Forms.Clipboard]::SetImage($bmp)"""
            )
            p.communicate()
        else:
            exception = None

            try:
                ImageGrab.grabclipboard()
            except IOError as e:
                exception = e

            self.assertIsInstance(exception, IOError)
            self.assertEqual(
                str(exception), "ImageGrab.grabclipboard() is macOS and Windows only"
            )
            return

        im = ImageGrab.grabclipboard()
        self.assert_image(im, im.mode, im.size)
