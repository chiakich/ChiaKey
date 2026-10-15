#!/usr/bin/env python3
# Rewrite a pkgbuild component plist in place so the payload always lands
# exactly where the package installs it (issue #25).
#
# BundleIsRelocatable=false keeps PackageKit from redirecting the payload onto
# another copy with the same bundle id -- pkgbuild's default emits <relocate>
# rules even though the top-level relocatable attribute reads "false".
# BundleIsVersionChecked=false makes the payload overwrite whatever is on disk:
# with the check on, PackageKit silently skips a destination holding a newer
# CFBundleVersion, and the postinstall placement check then has to report a
# failed install for a package that did nothing wrong.
import plistlib
import sys


def main(argv):
    if len(argv) != 2:
        print(
            "usage: force-component-nonrelocatable.py <component.plist>",
            file=sys.stderr,
        )
        return 2

    path = argv[1]
    with open(path, "rb") as handle:
        plist = plistlib.load(handle)

    # Recent pkgbuild emits an array of bundle entries; older ones keyed the
    # same dictionaries by payload path.
    entries = list(plist.values()) if isinstance(plist, dict) else plist
    if not entries:
        print(f"{path}: no bundle entries found", file=sys.stderr)
        return 1

    for entry in entries:
        entry["BundleIsRelocatable"] = False
        entry["BundleIsVersionChecked"] = False

    with open(path, "wb") as handle:
        plistlib.dump(plist, handle)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
