# MiniHMI / Mini PLC — Yocto Build Project

Reproducible Yocto build environment for the **MiniHMI** industrial
HMI + Mini PLC product running on Raspberry Pi.

This repo contains two things:

| Path             | What it is                                                          |
|------------------|---------------------------------------------------------------------|
| **`bin/`**       | Bootstrap script (`setup_yocto.sh`) + its docs                      |
| **`meta-miniplc/`** | Yocto layer with all product recipes, bbappends, configs           |

Everything else needed for a build — `poky/`, `meta-openembedded/`,
`meta-raspberrypi/` — is **not** checked in. They get cloned and pinned
to specific commits by `bin/setup_yocto.sh`. Same script on every
machine ⇒ same build tree, every time.

---

## TL;DR — fresh machine to running build

```bash
# 1. Clone this repo into the Yocto workspace location
git clone <this-repo-url> ~/Projects/Raspi/Yocto/yocto
cd ~/Projects/Raspi/Yocto/yocto

# 2. Run the bootstrap. It will:
#    - install host packages (Ubuntu/Debian)
#    - clone & pin poky / meta-openembedded / meta-raspberrypi
#    - seed build/conf/local.conf + bblayers.conf
#    - wire meta-miniplc/ into BBLAYERS
./bin/setup_yocto.sh

# 3. Build the image
source poky/oe-init-build-env build
bitbake rpi-test-image
```

Flashable image lands at:
```
build/tmp/deploy/images/raspberrypi/rpi-test-image-raspberrypi.wic.bz2
```

---

## Read next

- **`bin/README.md`** — full docs for the bootstrap script: pinned
  versions, host requirements, board re-targeting, troubleshooting,
  how to bump pinned commits.
- **`meta-miniplc/README.md`** — what's in the product layer, how it's
  organized, how to add new recipes / bbappends, history of why the
  layer exists.

---

## Design property worth knowing

`meta-raspberrypi/` is **pristine upstream** (commit `39e5dfd9cb`) —
we never patch it. Every product customization lives in
`meta-miniplc/` as a recipe or bbappend. Consequence: bumping the BSP
to a newer commit is a one-line edit to `setup_yocto.sh`, no merge
conflicts possible.

---

## License

MIT — see `meta-miniplc/COPYING.MIT`. Each pinned upstream layer
carries its own license; this repo doesn't redistribute their code.
