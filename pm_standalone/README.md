# pm — bulletproof Thorlabs PM101A reader

A tiny standalone tool that connects to the Thorlabs PM101A power meter **in any
case** and streams optical power. It exists because the PM101A's USBTMC firmware
is flaky on Linux — it can power up wedged and sometimes ignores transfers.

`pm` talks USBTMC directly over **libusb** (not the kernel `usbtmc` driver, which
fails on and wedges the PM101A) and self-heals by escalating through every
recovery method, in order, looping until the meter answers:

1. open + `*IDN?`
2. on failure → **USB bus reset** (`libusb_reset_device`) + retry (often clears it)
3. still failing → **VBUS power-cycle** the meter's hub port via `uhubctl` + retry

If the meter drops mid-stream, it reconnects the same way.

## Build
```bash
./build.sh          # needs: g++, pkg-config, libusb-1.0 dev
```
On Fedora: `sudo dnf install gcc-c++ libusb1-devel uhubctl`
On Debian/Pi: `sudo apt install g++ libusb-1.0-0-dev uhubctl`

## Run
```bash
./pm                # connects, then prints power in mW, Ctrl-C to stop
```

## Requirements for the last-resort power-cycle
The VBUS power-cycle fallback needs:
- the meter on a **hub that supports per-port power switching** (`uhubctl` lists it
  with `ppps`); verify with `sudo uhubctl`
- **passwordless sudo** for `uhubctl` (so it runs unattended):
  ```bash
  echo "$USER ALL=(root) NOPASSWD: $(command -v uhubctl)" | sudo tee /etc/sudoers.d/uhubctl-nopasswd
  sudo chmod 440 /etc/sudoers.d/uhubctl-nopasswd
  ```
- the kernel `usbtmc` driver blocked so it can't grab/wedge the meter:
  ```bash
  printf 'blacklist usbtmc\ninstall usbtmc /bin/true\n' | sudo tee /etc/modprobe.d/blacklist-usbtmc.conf
  sudo modprobe -r usbtmc
  ```

Even without a `ppps` hub, the bus-reset retries alone recover the meter most of
the time; the power-cycle is only the final fallback.

## Notes
- The meter's hub/port is **auto-detected** (searches `uhubctl` output for
  `1313:...`), so it works regardless of which port it's plugged into.
- Defaults: 1550 nm wavelength correction, 100-sample averaging, ~2 reads/sec.
