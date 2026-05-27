# Yocto Build Environment — MiniHMI / Mini PLC

`setup_yocto.sh` is a single-command bootstrap that turns a clean
Ubuntu/Debian machine into one that can build the MiniHMI firmware image
for Raspberry Pi. Layer commits are pinned, so every run on every machine
produces the exact same build tree.

This is the canonical way to reproduce the build environment when:

- you move to a new dev machine
- you onboard a new contributor
- you need a clean rebuild after a year-long pause
- a new Raspberry Pi variant joins the product line

---

## What this script sets up

| Component         | Source                                                                       |
|-------------------|------------------------------------------------------------------------------|
| Yocto release     | **kirkstone** (LTS 4.0.x)                                                    |
| poky              | cloned, pinned at `1ccf83e5d5` (`yocto-4.0.28-51-g1ccf83e5d5`)               |
| meta-openembedded | cloned, pinned at `06fc0278f1`                                               |
| meta-raspberrypi  | cloned, pinned at `39e5dfd9cb` — **pristine upstream, never modified**       |
| meta-miniplc      | **our product layer** — recipes + bbappends, not auto-cloned (see below)     |
| Init system       | SysV init (`update-rc.d`, no systemd)                                        |
| Default image     | `rpi-test-image` + `packagegroup-miniplc` (added by a bbappend in our layer) |
| Default MACHINE   | `raspberrypi` (RPi 1 / B+ / Zero — ARMv6)                                    |

Three pinned upstream trees plus one local product layer. The product
layer carries **every** MiniHMI customization — there is no patching of
`meta-raspberrypi` anywhere. That's the property that makes upstream
bumps painless.

After it finishes, the workspace looks like:

```
yocto/
├── bin/
│   ├── setup_yocto.sh        ← this script
│   └── README.md             ← this file
├── poky/                     ← cloned, pinned
├── meta-openembedded/        ← cloned, pinned
├── meta-raspberrypi/         ← cloned, pinned (pristine upstream)
├── meta-miniplc/             ← our product layer (recipes + bbappends)
└── build/
    └── conf/
        ├── local.conf        ← generated
        └── bblayers.conf     ← generated
```

---

## Host requirements

- **OS:** Ubuntu 20.04 / 22.04 LTS, or Debian 11+
  (other distros work but `setup_yocto.sh` won't auto-install host packages)
- **Disk:** ~50 GB free (`build/tmp` alone gets to ~30 GB)
- **RAM:** 8 GB minimum, 16 GB+ recommended
- **CPU:** anything — more cores = faster builds (BitBake parallelizes well)
- **Network:** active during the first build; ~5 GB of sources downloaded

Do **NOT** run BitBake as root — Yocto refuses.

---

## Quick start

```bash
# 1. Make sure the workspace dir exists (this script + bin/ + meta-miniplc/)
cd ~/Projects/Raspi/Yocto/yocto

# 2. Bring in meta-miniplc/ if it's not already there.
#    See "Product layer" below — rsync from another machine or clone once
#    it's been pushed to a remote. The script will print a HEADS-UP if it
#    notices the layer is missing.

# 3. Bootstrap (installs host packages, clones the pinned upstream layers,
#    seeds build/conf — meta-miniplc is wired into bblayers.conf for you)
./bin/setup_yocto.sh

# 4. Enter the build env and start a build
source poky/oe-init-build-env build
bitbake rpi-test-image
```

First build takes 1–3 hours on a fast desktop. Subsequent builds use the
sstate cache and are minutes.

The flashable image will be at:

```
build/tmp/deploy/images/raspberrypi/rpi-test-image-raspberrypi.wic.bz2
```

Flash with `bmaptool` (recommended) or `dd`:

```bash
bmaptool copy --bmap rpi-test-image-raspberrypi.wic.bmap \
              rpi-test-image-raspberrypi.wic.bz2 /dev/sdX
```

---

## Targeting a different Raspberry Pi

`MACHINE` is overridable at script invocation. Valid values come from
`meta-raspberrypi/conf/machine/*.conf`:

| Board                       | `MACHINE` value           |
|-----------------------------|---------------------------|
| RPi 1 / B+ / Zero / Zero W  | `raspberrypi` *(default)* |
| RPi 2                       | `raspberrypi2`            |
| RPi 3 (32-bit)              | `raspberrypi3`            |
| RPi 3 / Zero 2 W (64-bit)   | `raspberrypi3-64`         |
| RPi 4 (32-bit)              | `raspberrypi4`            |
| RPi 4 / 400 (64-bit)        | `raspberrypi4-64`         |
| RPi 5 (64-bit)              | not in kirkstone — needs scarthgap+ |

```bash
MACHINE=raspberrypi4-64 ./bin/setup_yocto.sh
```

This regenerates `build/conf/local.conf` (only if conf wasn't already
seeded — see below).

---

## Product layer (`meta-miniplc/`)

All MiniHMI product code (`miniplc-hmi`, `plc-firmware`, `mini-plc-web`,
`packagegroup-miniplc`) and every override against the BSP layers
(`rpi-test-image`, `packagegroup-rpi-test`, `rpi-config`, `nginx`,
`lvgl`, `init-ifupdown`, `sysvinit-inittab`) lives in `meta-miniplc/` —
a self-contained Yocto layer **alongside** `meta-raspberrypi/`, never
inside it.

Two consequences worth knowing:

- `meta-raspberrypi/` stays pristine at the pinned upstream commit. You
  can bump it for new kernels / firmware by just editing `RPI_COMMIT`
  in `setup_yocto.sh` and re-running — no merge conflicts possible,
  because we don't touch any file in that tree.
- The same `meta-miniplc/` rides along onto any future board variant.
  Switch `MACHINE`, re-build; the BSP layer does the heavy lifting and
  our product layer comes along unchanged.

See `meta-miniplc/README.md` for the layer's internal structure and
how to add new recipes.

### How `meta-miniplc/` arrives on a new machine

`setup_yocto.sh` already lists `$YOCTO_ROOT/meta-miniplc` in
`bblayers.conf`, but it does **not** clone the layer — there isn't a
public URL for it yet. On a brand-new machine you bring it in one of
these ways:

**Option A — copy it from a machine that already has it (today's
default):**

```bash
rsync -av <other-machine>:~/Projects/Raspi/Yocto/yocto/meta-miniplc/ \
          ~/Projects/Raspi/Yocto/yocto/meta-miniplc/
```

**Option B — once you push it to git:**

1. `git init` inside `meta-miniplc/` and push to a remote (e.g.
   `github.com/tganeshg/meta-miniplc`).
2. Add a `clone_or_pin` call inside `fetch_layers()` in
   `setup_yocto.sh`, mirroring the existing entries for poky /
   meta-oe / meta-raspberrypi. Pin the commit too.
3. Re-run the script everywhere.

If `meta-miniplc/` is missing when `bitbake` parses, it'll fail at
`packagegroup-miniplc` (and at every bbappend that targets a recipe in
that layer) — the script prints a HEADS-UP banner pointing you here
when it notices the directory is absent.

---

## Re-running the script

`setup_yocto.sh` is idempotent:

- Existing layer checkouts are fast-forwarded (or detached) to the
  pinned commit. Local uncommitted changes are preserved as a worktree
  diff; you get a warning.
- `build/conf/local.conf` and `bblayers.conf` are **left alone** if they
  already exist. Delete both to re-seed:

  ```bash
  rm build/conf/local.conf build/conf/bblayers.conf
  ./bin/setup_yocto.sh
  ```

---

## Environment variables

| Variable          | Default          | What it does                                  |
|-------------------|------------------|-----------------------------------------------|
| `MACHINE`         | `raspberrypi`    | Yocto MACHINE value (see board table above)   |
| `IMAGE`           | `rpi-test-image` | Image recipe name (just used in printed help) |
| `SKIP_HOST_PKGS`  | `0`              | `1` = don't run `apt-get install`             |

---

## Bumping pinned versions

When you want to move to newer layer commits (e.g. picking up a
security fix in poky), edit the `*_COMMIT` variables at the top of
`setup_yocto.sh`, commit the change, then re-run the script on every
build machine. Same one-line change, same outcome everywhere.

`meta-miniplc` is **not** pinned by the script today — it's a local
directory you manage by hand (or via its own git repo if you've pushed
it). Once it lives at a published URL, add a `clone_or_pin` call for
it in `fetch_layers()` and a `MINIPLC_COMMIT` next to the others.

Don't move forward on commits without testing — kirkstone is the LTS we
target, but layer drift across kirkstone tip is a real source of
breakage. And because all our overrides live in `meta-miniplc` (not
inside `meta-raspberrypi`), the worst case for a BSP bump is a build
failure, never a merge conflict.

---

## Troubleshooting

**Clone of poky or meta-openembedded stalls** — the script uses HTTPS
by default (universally reachable). If you're behind a strict proxy,
either configure git's `http.proxy`/`https.proxy` or switch the URLs
to the `git://` variants (port 9418, usually open on home networks but
blocked by corporate firewalls):

```
POKY_URL="git://git.yoctoproject.org/poky"
OE_URL="git://git.openembedded.org/meta-openembedded"
```

**"Locale is set to C" warning during bitbake** — the script runs
`locale-gen en_US.UTF-8`; if it's still missing, do it manually:

```bash
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8
```

**Out of disk while building** — `BB_DISKMON_DIRS` will pause/halt the
build before damage. Free space and resume with the same `bitbake`
command; sstate makes it cheap.

**`bitbake` complains about Python or `m4`** — re-run with
`SKIP_HOST_PKGS=0` to refresh the host package set.

**`bitbake` fails parsing with "ParseError ... packagegroup-miniplc"
or "Unable to find ... miniplc-hmi"** — `meta-miniplc/` is missing or
not at `$YOCTO_ROOT/meta-miniplc`. The directory must exist before
`bitbake` runs. See the "Product layer" section for how to bring it in.

**A `meta-raspberrypi/` file shows up as modified in `git status`** —
that's a regression. Nothing in our workflow should ever edit a file
under `meta-raspberrypi/`; all changes go in `meta-miniplc/` as a
bbappend. Revert the edit (`git -C meta-raspberrypi checkout -- <file>`)
and re-do the change as a bbappend in the product layer.
