# WoL Server - Multi-platform Wake-on-LAN Server

A lightweight, cross-platform Wake-on-LAN server written in C.

## Features

- Zero dependencies (statically compiled)
- Small binary size (~100KB)
- Web interface for easy use
- REST API for automation
- Cross-platform support

## Supported Platforms

- Linux x86_64
- Linux ARM64 (aarch64)
- Linux ARMv7
- Linux ARMv6
- Linux RISC-V
- Windows x64
- macOS x64
- macOS ARM64

## Usage

### Web Interface
Access `http://your-server:8044/` for the web interface.

### API
```bash
curl "http://your-server:8044/wol?mac=b6-6f-9c-cc-d7-99&ip=192.168.31.255"
