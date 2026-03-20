# InfraFi

Transmit WiFi credentials from a **Flipper Zero** to a **Linux server** using infrared. Point, press Send, connected.

Built for headless servers (NAS boxes, Intel NUCs, etc.) where typing WiFi passwords is painful or impossible.

## How It Works

```mermaid
flowchart LR
    subgraph Flipper Zero
        A[InfraFi App]
        A1[Manual Entry]
        A2[NFC Tag Scan]
        A3[Saved Networks]
        A1 --> A
        A2 --> A
        A3 --> A
    end

    subgraph Linux Server
        B[infrafid daemon]
        B1[/dev/lirc0]
        B2[WiFi Connect]
        B1 --> B --> B2
    end

    A -- "IR (RC-6, 36kHz)" --> B1
```

The Flipper encodes WiFi credentials as a sequence of **RC-6 IR messages** and blasts them at the server's CIR (Consumer IR) receiver. The `infrafid` daemon decodes the transmission and connects to the network automatically. No pairing, no Bluetooth, no network required — just line-of-sight IR.

## Features

### Flipper Zero App
- **Manual entry** — On-screen keyboard for SSID and password, security type selector (Open/WPA/WEP/SAE)
- **NFC WiFi tags** — Scan an NTAG213/215/216 tag with WiFi credentials (standard NDEF WiFi Simple Configuration format) and transmit instantly
- **Saved networks** — Credentials auto-save to SD card after successful transmit. Browse, resend, or delete saved networks
- **Fast transmission** — Full credentials sent in under a second via RC-6 protocol
- **Hidden network support** — Toggle hidden SSID flag

### Linux Daemon (`infrafid`)
- **Zero dependencies** — Pure C, no Python or runtime libraries needed
- **Automatic WiFi connection** — Detects NetworkManager, systemd-networkd, or ifupdown and connects appropriately
- **Rollback on failure** — Saves current SSID before connecting; reconnects to previous network if the new one fails
- **Runs as a service** — systemd unit with auto-restart, logs to journald/syslog
- **ITE8708 optimized** — Uses `LIRC_MODE_SCANCODE` for kernel-decoded RC-6, avoiding hardware FIFO overflow issues with the CIR receivers found in Intel NUCs

## Getting Started

### Requirements

**Flipper Zero:**
- Flipper Zero with up-to-date firmware
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (Flipper build tool)

**Linux Server:**
- IR receiver (tested with ITE8708 CIR in Intel NUCs)
- `/dev/lirc0` device available
- `gcc` and Linux headers for building
- NetworkManager, systemd-networkd, or ifupdown + wpa_supplicant for WiFi management

### Build & Install — Flipper App

```bash
# Clone the repo
git clone https://github.com/amd989/infrafi.git
cd infrafi

# Build
ufbt

# Deploy to Flipper (connected via USB)
ufbt launch
```

### Build & Install — Linux Daemon

```bash
cd daemon

# Quick install (builds, installs, configures RC-6, starts service)
sudo ./install.sh

# Or manually:
make
sudo make install
sudo systemctl enable --now infrafid
```

The install script automatically:
- Configures the IR receiver for RC-6 protocol only
- Creates a udev rule so the config persists across reboots
- Installs and starts the systemd service

### Verify IR Receiver

```bash
# Check that rc-6 is the active protocol
cat /sys/class/rc/rc0/protocols
# Should show: ... [rc-6] ...

# Test reception (Ctrl+C to stop)
ir-keytable -t -s rc0
```

## Usage

### Manual Entry
1. Open **InfraFi** on your Flipper
2. Select **Send Credentials**
3. Enter SSID, password, and security type
4. Review on the confirm screen, press **Send**
5. Point the Flipper at the server's IR receiver

### NFC Tag
1. Write WiFi credentials to an NTAG213/215/216 using a phone app (e.g., **NFC Tools**)
2. Open **InfraFi** → **Scan NFC Tag**
3. Hold the tag to the back of the Flipper
4. Review credentials, press **Send**

### Saved Networks
1. Previously transmitted networks are auto-saved to the SD card
2. Open **InfraFi** → **Saved** to browse them
3. Select a network to resend

### Daemon

```bash
# Run in foreground with verbose logging (useful for testing)
sudo infrafid -f -v

# Check service status
sudo systemctl status infrafid

# Watch logs
sudo journalctl -u infrafid -f
```

## Protocol

InfraFi uses **RC-6 Mode 0** IR protocol at 36kHz — the same protocol used by standard media center remotes. This is intentional: CIR receivers like the ITE8708 (found in Intel NUCs) have hardware decoders optimized for RC-6. Using the kernel's built-in RC-6 decoder (`LIRC_MODE_SCANCODE`) avoids the tiny hardware FIFO that overflows with custom raw protocols.

Each RC-6 message carries one byte of payload:

| Field | Bits | Description |
|-------|------|-------------|
| Magic | `addr[7:4]` | `0xA` — identifies InfraFi messages |
| Frame type | `addr[3:2]` | `00`=START, `01`=DATA, `10`=END |
| Pass | `addr[1:0]` | Retransmission attempt (0-3) |
| Command | `cmd[7:0]` | Payload byte |

**Message sequence:** `START(len)` → `DATA × N` → `END(crc8)`

The payload is a standard WiFi QR string: `WIFI:T:WPA;S:MyNetwork;P:MyPassword;H:false;;`

## Project Structure

```
infrafi/
├── application.fam              # Flipper app manifest
├── flipper/                     # Flipper Zero app
│   ├── wi_fir.h/c               # App entry, ViewDispatcher + SceneManager
│   ├── wfr_encode.h/c           # RC-6 IR encoder + transmitter
│   ├── wfr_nfc.h/c              # NFC NDEF WiFi tag parser
│   ├── wfr_storage.h/c          # SD card credential storage
│   ├── protocol/
│   │   ├── wfr_protocol.h       # Shared protocol constants + structs
│   │   └── wfr_protocol.c       # CRC-8, WiFi string builder/parser
│   ├── scenes/                  # UI scenes (menu, editors, confirm, transmit, NFC, saved, about)
│   └── images/                  # App icon
└── daemon/                      # Linux daemon (infrafid)
    ├── main.c                   # Entry point, CLI args, main loop
    ├── wfr_lirc.h/c             # LIRC scancode reader
    ├── wfr_decode.h/c           # RC-6 message reassembler
    ├── wfr_network.h/c          # WiFi connector (NM/networkd/ifupdown) with rollback
    ├── Makefile                 # Build
    ├── infrafid.service         # systemd unit
    └── install.sh               # One-step install
```

## Author

**Alejandro Mora** — [github.com/amd989](https://github.com/amd989)

## License

[MIT](LICENSE.md)
