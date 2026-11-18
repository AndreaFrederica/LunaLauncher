"""Probe the real packaged AppImage's MCP update API using disposable user data."""
import argparse
import json
import os
from pathlib import Path
import tempfile

from api_sidecar_smoke import Sidecar


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--online", action="store_true")
    parser.add_argument("--portable", action="store_true", help="Probe a deployed portable AppRun instead of an AppImage")
    args = parser.parse_args()
    # This path works on Linux hosts without FUSE, too; it must still identify
    # its outer image and offer the correct AppImage update capabilities.
    if not args.portable:
        os.environ["APPIMAGE_EXTRACT_AND_RUN"] = "1"
    os.environ["PATH"] = "/usr/bin:/bin"
    for key in ("LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "APPDIR", "APPIMAGE"):
        os.environ.pop(key, None)
    with tempfile.TemporaryDirectory(prefix="luna-appimage-api-") as temporary:
        profile = Path(temporary)
        sidecar = Sidecar(args.image.resolve(), profile)
        try:
            sidecar.call("initialize")
            status = sidecar.ok("launcher.update.status")
            assert status["supported"] is True, status
            assert Path(status["dataRoot"]) == profile, status
            settings = sidecar.ok("launcher.update.configure", automatic=False, intervalSeconds=3600)
            assert settings["automatic"] is False and settings["intervalSeconds"] == 3600, settings
            invalid = sidecar.execute("launcher.update.apply", versionTag="../invalid")
            assert invalid["ok"] is False, invalid
            result = {"supported": True, "dataRootPreserved": True, "preferences": True, "invalidApplyRejected": True}
            if args.online:
                releases = sidecar.ok("launcher.update.releases")
                result["releaseCatalog"] = releases
            print(json.dumps(result, ensure_ascii=False))
        finally:
            sidecar.close()


if __name__ == "__main__":
    main()
