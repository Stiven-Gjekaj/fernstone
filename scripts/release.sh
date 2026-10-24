#!/bin/sh
# Make a release of the version in core/fern_version.h.
# Usage: scripts/release.sh
# It runs the tests on this Mac and on the phone, builds the .deb (iOS 6),
# the .ipa (iOS), and the zip of the Mac app, pushes the commits and the
# tag, publishes the GitHub release with the notes of CHANGELOG.md, adds the
# .deb to the Cydia repository in ../cydia, adds the app to the SideStore
# source in ../sidestore-source, and closes the milestone of the version.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CYDIA=${CYDIA_REPO:-$ROOT/../cydia}
SIDESTORE=${SIDESTORE_REPO:-$ROOT/../sidestore-source}
REPO=Stiven-Gjekaj/fernstone
VERSION=$(sed -n 's/#define FERN_APP_VERSION "\(.*\)"/\1/p' "$ROOT/core/fern_version.h")
TAG="v$VERSION"
DEB="io.github.stivengjekaj.fernstone_${VERSION}_iphoneos-arm.deb"
cd "$ROOT"

[ -z "$(git status --porcelain)" ] || { echo "release: commit the changes first" >&2; exit 1; }
grep -q "^## $VERSION " CHANGELOG.md || { echo "release: CHANGELOG.md has no entry for $VERSION" >&2; exit 1; }
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    echo "release: the tag $TAG is already there" >&2
    exit 1
fi

mkdir -p build
scripts/test.sh >build/release-tests.log || { cat build/release-tests.log >&2; exit 1; }
tail -1 build/release-tests.log
scripts/test.sh --phone >build/release-tests.log || { cat build/release-tests.log >&2; exit 1; }
tail -1 build/release-tests.log
scripts/deb.sh >/dev/null
scripts/ios.sh --ipa >/dev/null
scripts/mac.sh >/dev/null
(cd build/macos && rm -f ../Fernstone-macOS.zip && ditto -c -k --keepParent Fernstone.app ../Fernstone-macOS.zip)

git push -q
git tag -a "$TAG" -m "Fernstone $VERSION"
git push -q origin "$TAG"
awk -v v="## $VERSION " 'index($0, v) == 1 {f = 1; next} /^## / {f = 0} f' CHANGELOG.md >build/notes.md
cat >>build/notes.md <<NOTES

Install on iOS 6: copy the .deb to the phone and run \`dpkg -i $DEB\` as root.
Install on iOS 15 or later: add Fernstone.ipa to SideStore or LiveContainer.
Install on macOS: unzip Fernstone-macOS.zip and open Fernstone.app.
Each build needs your own client jar of the version that you play. The
release holds no file of Mojang.
NOTES
gh release create "$TAG" -R "$REPO" -t "Fernstone $VERSION" -F build/notes.md \
    "build/$DEB" build/Fernstone.ipa build/Fernstone-macOS.zip

# The Cydia repository: add the package, and show the new version.
(
    cd "$CYDIA"
    scripts/update.sh "$ROOT/build/$DEB"
    git add debs Packages Packages.bz2 Packages.gz Release
    git commit -qm "feat: add a new release of Fernstone"
    export ROW="| Fernstone (\`io.github.stivengjekaj.fernstone\`) | $VERSION | iOS 6 or newer |"
    if grep -q '^| Fernstone (' README.md; then
        sed -i '' "s/^| Fernstone (\`io.github.stivengjekaj.fernstone\`) | [0-9.]* |/| Fernstone (\`io.github.stivengjekaj.fernstone\`) | $VERSION |/" README.md
    else
        # The first release adds a row under the last row of the table.
        perl -0pi -e 's/((?:^\| \S+ \(`io\.github[^\n]*\n)+)/$1$ENV{ROW}\n/m' README.md
    fi
    git add README.md
    git commit -qm "docs: list the new release of Fernstone"
    git push -q
)

# The SideStore source: add the app the first time. The workflow of that
# repository builds apps.json from the GitHub releases, so a new release
# shows there within about one hour.
(
    cd "$SIDESTORE"
    if [ ! -f apps/fernstone.json ]; then
        cp "$ROOT/packaging/sidestore.json" apps/fernstone.json
        git add apps/fernstone.json
        git commit -qm "feat: add Fernstone"
        git push -q
    fi
)

# Close the issues of the milestone, then the milestone.
MILESTONE=$(gh api "repos/$REPO/milestones?state=open" -q ".[] | select(.title | startswith(\"$TAG\")) | .number")
if [ -n "$MILESTONE" ]; then
    TITLE=$(gh api "repos/$REPO/milestones/$MILESTONE" -q .title)
    for issue in $(gh issue list -R "$REPO" -m "$TITLE" --json number -q '.[].number'); do
        gh issue close "$issue" -R "$REPO" -c "Done in $TAG." >/dev/null
    done
    gh api -X PATCH "repos/$REPO/milestones/$MILESTONE" -f state=closed -q '.title + ": " + .state'
fi
echo "released $TAG"
