#!/usr/bin/env bash
#
# setup_yocto.sh — Bootstrap the MiniHMI / Mini PLC Yocto build environment.
#
# Goal: a single command that turns a clean Ubuntu/Debian machine into one
# that can `bitbake rpi-test-image` for the MiniHMI product. Layer commits
# are pinned so every run on every machine produces the exact same tree.
#
# Idempotent — safe to re-run. Existing layer checkouts are fast-forwarded
# (or reset) to the pinned commit; build/conf is left alone if already
# initialized (delete it to re-seed).
#
# Usage:
#     ./bin/setup_yocto.sh                    # default: RPi B+ (raspberrypi)
#     MACHINE=raspberrypi3-64 ./bin/setup_yocto.sh
#     SKIP_HOST_PKGS=1 ./bin/setup_yocto.sh   # skip apt-get install step
#
set -euo pipefail

# ---------- Pinned layer versions ----------------------------------------
# These are the commits the live build was last verified against.
# Bump them deliberately when upgrading; the whole point is reproducibility.

POKY_URL="https://git.yoctoproject.org/poky"
POKY_BRANCH="kirkstone"
POKY_COMMIT="1ccf83e5d561a2876ea648cc3505ab35511a2c0d"   # yocto-4.0.28-51-g1ccf83e5d5

OE_URL="https://git.openembedded.org/meta-openembedded"
OE_BRANCH="kirkstone"
OE_COMMIT="06fc0278f10d630838d703dde707bbf0e2999873"

RPI_URL="https://github.com/agherzan/meta-raspberrypi.git"
RPI_BRANCH="kirkstone"
RPI_COMMIT="39e5dfd9cb8f11eda189d0beb62d27c83fc57e75"

# Target board — RPi B+ is ARMv6 = MACHINE "raspberrypi"
MACHINE="${MACHINE:-raspberrypi}"
IMAGE="${IMAGE:-rpi-test-image}"

# ---------- Workspace layout --------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YOCTO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$YOCTO_ROOT/build"

# ---------- Pretty output -----------------------------------------------
if [ -t 1 ]; then
    C_BLUE="\033[1;34m"; C_YELLOW="\033[1;33m"; C_RED="\033[1;31m"
    C_GREEN="\033[1;32m"; C_RESET="\033[0m"
else
    C_BLUE=""; C_YELLOW=""; C_RED=""; C_GREEN=""; C_RESET=""
fi
info() { printf "${C_BLUE}[setup]${C_RESET} %s\n" "$*"; }
warn() { printf "${C_YELLOW}[warn]${C_RESET}  %s\n" "$*"; }
die()  { printf "${C_RED}[err]${C_RESET}   %s\n" "$*" >&2; exit 1; }
ok()   { printf "${C_GREEN}[ok]${C_RESET}    %s\n" "$*"; }

# ---------- Sanity checks -----------------------------------------------
[ "$(id -u)" -eq 0 ] && die "do NOT run this as root — Yocto refuses root builds"

if [ -z "${BASH_VERSION:-}" ]; then
    die "run with bash, not sh: bash $0"
fi

# ---------- Step 1: install host packages -------------------------------
install_host_pkgs() {
    if ! command -v apt-get >/dev/null 2>&1; then
        warn "This script auto-installs host deps on Debian/Ubuntu only."
        warn "On other distros, follow the Yocto host requirements page:"
        warn "  https://docs.yoctoproject.org/4.0/ref-manual/system-requirements.html"
        return 0
    fi

    info "Installing Yocto host packages (sudo will prompt for password)"
    sudo apt-get update -qq
    # Package list = Yocto 4.0 "Essentials" + a few extras we know are needed.
    sudo apt-get install -y --no-install-recommends \
        gawk wget git diffstat unzip texinfo gcc build-essential chrpath \
        socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
        iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev \
        python3-subunit mesa-common-dev zstd liblz4-tool file locales \
        bzip2 lz4
    # Yocto needs a UTF-8 locale or some recipes fail in obscure ways.
    sudo locale-gen en_US.UTF-8 >/dev/null 2>&1 || true
    ok "host packages installed"
}

# ---------- Step 2: clone / pin meta-layers ------------------------------
clone_or_pin() {
    local url="$1" branch="$2" commit="$3" dest="$4"
    local name
    name="$(basename "$dest")"

    if [ -d "$dest/.git" ]; then
        info "updating $name -> ${commit:0:10}"
        (
            cd "$dest"
            # Be tolerant of mirrors that lack the exact branch tip
            git fetch --quiet origin "$branch" || git fetch --quiet origin
            # Detach to the pinned commit; preserve any uncommitted work
            if ! git diff --quiet HEAD -- 2>/dev/null; then
                warn "$name has uncommitted changes — leaving them in place"
                warn "$name HEAD will be moved to $commit, your edits stay as worktree diff"
            fi
            git checkout --quiet --detach "$commit"
        )
    else
        info "cloning $name (branch=$branch)"
        git clone --quiet --branch "$branch" "$url" "$dest"
        ( cd "$dest" && git checkout --quiet --detach "$commit" )
    fi
    ok "$name pinned at $(cd "$dest" && git rev-parse --short HEAD)"
}

fetch_layers() {
    clone_or_pin "$POKY_URL" "$POKY_BRANCH" "$POKY_COMMIT" "$YOCTO_ROOT/poky"
    clone_or_pin "$OE_URL"   "$OE_BRANCH"   "$OE_COMMIT"   "$YOCTO_ROOT/meta-openembedded"
    clone_or_pin "$RPI_URL"  "$RPI_BRANCH"  "$RPI_COMMIT"  "$YOCTO_ROOT/meta-raspberrypi"
}

# ---------- Step 3: seed build/conf --------------------------------------
seed_conf() {
    if [ -f "$BUILD_DIR/conf/local.conf" ] && [ -f "$BUILD_DIR/conf/bblayers.conf" ]; then
        info "build/conf already initialized — leaving it alone"
        info "(delete $BUILD_DIR/conf/{local,bblayers}.conf to re-seed)"
        return 0
    fi

    info "initializing build directory via oe-init-build-env"
    # oe-init-build-env wants to be sourced; do it in a subshell so its
    # environment churn doesn't leak into ours.
    (
        set +eu
        cd "$YOCTO_ROOT"
        # shellcheck disable=SC1091
        source "$YOCTO_ROOT/poky/oe-init-build-env" "$BUILD_DIR" >/dev/null
    )

    info "writing bblayers.conf"
    cat > "$BUILD_DIR/conf/bblayers.conf" <<EOF
# POKY_BBLAYERS_CONF_VERSION is increased each time build/conf/bblayers.conf
# changes incompatibly
POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "\${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \\
  $YOCTO_ROOT/poky/meta \\
  $YOCTO_ROOT/poky/meta-poky \\
  $YOCTO_ROOT/poky/meta-yocto-bsp \\
  $YOCTO_ROOT/meta-raspberrypi \\
  $YOCTO_ROOT/meta-openembedded/meta-oe \\
  $YOCTO_ROOT/meta-openembedded/meta-webserver \\
  $YOCTO_ROOT/meta-openembedded/meta-networking \\
  $YOCTO_ROOT/meta-openembedded/meta-python \\
  $YOCTO_ROOT/meta-miniplc \\
  "
EOF

    info "writing local.conf (MACHINE=$MACHINE)"
    cat > "$BUILD_DIR/conf/local.conf" <<EOF
MACHINE = "$MACHINE"
DISTRO ?= "poky"

# SysV init: register /etc/init.d scripts via update-rc.d.
# (We're not on systemd; journalctl won't exist on target — use logread / init.d.)
DISTRO_FEATURES:append = " sysvinit"
PACKAGE_CLASSES ?= "package_deb"
EXTRA_IMAGE_FEATURES ?= "debug-tweaks"

USER_CLASSES ?= "buildstats"
PATCHRESOLVE = "noop"

BB_DISKMON_DIRS ??= "\\
    STOPTASKS,\${TMPDIR},1G,100K \\
    STOPTASKS,\${DL_DIR},1G,100K \\
    STOPTASKS,\${SSTATE_DIR},1G,100K \\
    STOPTASKS,/tmp,100M,100K \\
    HALT,\${TMPDIR},100M,1K \\
    HALT,\${DL_DIR},100M,1K \\
    HALT,\${SSTATE_DIR},100M,1K \\
    HALT,/tmp,10M,1K"

CONF_VERSION = "2"

# RPi: enable the UART console (gives us a /dev/ttyAMA0 login on header pins).
ENABLE_UART = "1"
EOF
    ok "build/conf seeded"
}

# ---------- Step 4: warn about product layer -----------------------------
# meta-miniplc holds the MiniHMI product recipes and all our bbappends
# against upstream meta-raspberrypi / meta-oe. We don't clone it from a
# remote (yet) — it's expected to be present at $YOCTO_ROOT/meta-miniplc.
# Once it's hosted at a public URL, add a clone_or_pin call alongside
# the others in fetch_layers().
warn_product_layer() {
    local miniplc_layer="$YOCTO_ROOT/meta-miniplc"
    if [ ! -d "$miniplc_layer" ]; then
        cat <<EOF

${C_YELLOW}HEADS UP${C_RESET}
The MiniHMI product layer is missing:
    $miniplc_layer

It contains:
  - the three product recipes (miniplc-hmi, plc-firmware, mini-plc-web)
  - bbappends that customize rpi-test-image, packagegroup-rpi-test,
    rpi-config, nginx, lvgl, init-ifupdown, sysvinit-inittab

To bring it in:
  a) (when published) clone it next to poky/meta-raspberrypi, e.g.
       git clone <your-fork>/meta-miniplc $miniplc_layer
  b) (today) copy it from another machine that has it.

bblayers.conf already lists this path, so as soon as the directory
exists \`bitbake rpi-test-image\` will pick it up.

EOF
    fi
}

# ---------- Run ----------------------------------------------------------
info "MiniHMI / Mini PLC Yocto bootstrap"
info "  workspace : $YOCTO_ROOT"
info "  machine   : $MACHINE"
info "  image     : $IMAGE"
echo

if [ "${SKIP_HOST_PKGS:-0}" != "1" ]; then
    install_host_pkgs
fi
fetch_layers
seed_conf
warn_product_layer

cat <<EOF

==========================================================================
${C_GREEN}Setup complete.${C_RESET}

To enter the build environment and start a build:

    cd $YOCTO_ROOT
    source poky/oe-init-build-env build
    bitbake $IMAGE

First build will pull a lot of sources and take 1-3 hours on a beefy
machine, longer on a laptop. Subsequent builds are sstate-cached.

The flashable image lands at:
    $BUILD_DIR/tmp/deploy/images/$MACHINE/$IMAGE-$MACHINE.wic.bz2

Flash to an SD card with:
    bmaptool copy --bmap <image>.wic.bmap <image>.wic.bz2 /dev/sdX

To re-target a different board (e.g. RPi 3B / Zero 2W):
    MACHINE=raspberrypi3-64 $0
==========================================================================
EOF
