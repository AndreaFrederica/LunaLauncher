import argparse
import glob
import io
import os.path
import pathlib
import re
import subprocess

import PIL.features
import PIL.Image
import PIL.ImageFile
import numpy

if not PIL.features.check("webp"):
    raise RuntimeError("This Pillow does not come with WebP support.")


# Define our custom PAM image reader (minimize dependencies)
class PAMImageFile(PIL.ImageFile.ImageFile):
    format = "PAM"
    format_description = "Netpbm PAM (RGB_ALPHA tuple subset)"

    @staticmethod
    def _accept(prefix: bytes):
        return prefix.startswith(b"P7\n") or prefix.startswith(b"P7\r\n")

    def _open(self) -> None:
        assert self.fp is not None

        test = self.fp.read(1024)  # Just read first 1KiB
        if not PAMImageFile._accept(test):
            raise SyntaxError("Not PAM")

        endhdr_where = test.find(b"ENDHDR")
        if endhdr_where == -1:
            raise SyntaxError("Missing ENDHDR")

        header = test[:endhdr_where]

        width_match = re.search(rb"WIDTH (\d+)", header)
        if width_match is None:
            raise SyntaxError("Missing WIDTH")
        width = int(width_match.group(1))

        height_match = re.search(rb"HEIGHT (\d+)", header)
        if height_match is None:
            raise SyntaxError("Missing HEIGHT")
        height = int(height_match.group(1))

        if b"DEPTH 4" not in header:
            raise SyntaxError("Missing DEPTH or DEPTH is not 4")
        if b"MAXVAL 255" not in header:
            raise SyntaxError("Missing MAXVAL or MAXVAL is not 255")
        if b"TUPLTYPE RGB_ALPHA" not in header:
            raise SyntaxError("Missing TUPLTYPE or TUPLTYPE is not RGB_ALPHA")

        if test[endhdr_where + 6] == 13:
            # \r\n
            start_image_data = endhdr_where + 8
        else:
            # \n
            start_image_data = endhdr_where + 7

        self._size = (width, height)
        self._mode = "RGBA"
        self.tile = [PIL.ImageFile._Tile("raw", (0, 0, width, height), start_image_data, ("RGBA", 0, 1))]


# Register our custom PAM decoder
PIL.Image.register_open("PAM", PAMImageFile, PAMImageFile._accept)
PIL.Image.register_extensions("PAM", [".pam"])

SCRIPT_DIR = pathlib.Path(os.path.dirname(__file__))
TEST_PROGRAM = SCRIPT_DIR / ("a.exe" if os.name == "nt" else "a.out")
CHANNEL_INDICES_MAP = ["R", "G", "B", "A"]


def compare_image(webp: str, pam_stdout: bytes):
    image_ref = numpy.array(PIL.Image.open(webp, "r", ["webp"]).convert("RGBA"), numpy.uint8)

    with io.BytesIO(pam_stdout) as f:
        image = numpy.array(PIL.Image.open(f, "r", ["PAM"]).convert("RGBA"), numpy.uint8)

    errors: list[str] = []
    if image_ref.shape != image.shape:
        errors.append(
            f"Image size mismatch. Expected {image_ref.shape[1]}x{image_ref.shape[0]}, got {image.shape[1]}x{image.shape[0]}"
        )
        return False, errors

    # Compare pixels
    for y in range(image.shape[0]):
        for x in range(image.shape[1]):
            ia = image_ref[y, x]
            ib = image[y, x]

            if numpy.any(ia != ib) and (ia[3] > 0 or ib[0] > 0):
                err: list[str] = [f"Pixel at {x}x{y}:"]

                for i, cname in enumerate(CHANNEL_INDICES_MAP):
                    if ia[i] != ib[i]:
                        err.append(f"Expected {cname}={ia[i]}, got {cname}={ib[i]}.")

                errors.append(" ".join(err))

    return errors


def main():
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("path", type=pathlib.Path, default=SCRIPT_DIR / "data" / "*.webp", nargs="?")
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args()

    bad = False
    summary: list[tuple[str, bool]] = []

    print("----------------------------------------")
    for path in glob.glob(str(args.path)):
        print(path, end="", flush=True)
        process = subprocess.run([TEST_PROGRAM, path], text=False, capture_output=True)

        if process.returncode == 0:
            errors = compare_image(path, process.stdout)

            if errors:
                print(" ❌")
                bad = True
                summary.append((path, False))

                if args.full:
                    for err in errors:
                        print(err)
                else:
                    for err in errors[:5]:
                        print(err)
                    if len(errors) > 5:
                        print("(and many more)")
            else:
                print(" ✅")
                summary.append((path, True))
        else:
            print(f" ❌")
            bad = True
            summary.append((path, False))

            print("Process exited with code", process.returncode)
            print("stderr:")
            try:
                print(str(process.stderr, "utf-8"))
            except UnicodeDecodeError:
                print("(cannot be converted to UTF-8)")

        print("----------------------------------------")

    fields = [0, 0]
    print("Summary:")
    for path, outcome in summary:
        print(path, "✅" if outcome else "❌")
        fields[outcome] += 1

    print("Success:", fields[1], "Failure:", fields[0])
    return bad


if __name__ == "__main__":
    import sys

    sys.exit(main())
