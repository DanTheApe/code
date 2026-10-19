#!/bin/bash
exec /home/dan/.local/share/ada-microbit/venv/bin/pyocd gdbserver --port 3333 --target nrf52833

