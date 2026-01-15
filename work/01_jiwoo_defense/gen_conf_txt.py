#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Generate GNSS-SDR CHANNELS CONFIG block for dual-RF (RF0 / RF1) tracking.
Each PRN is duplicated so both RF boards track the same satellite.

Usage:
    python3 generate_channels_block.py 3 5 7 10

This prints a full CHANNEL CONFIG block including:
    - Section header
    - Channels count
    - Paired channels (0,1), (2,3), ...
"""

def generate_channels_block(prns, signal="1C"):
    n_prns = len(prns)
    total_channels = n_prns * 2  # RF0/RF1 per PRN

    out = []
    out.append("############################################################")
    out.append("# CHANNELS CONFIG")
    out.append("############################################################")
    out.append(f"Channels_1C.count={total_channels}")
    out.append("Channels.in_acquisition=1\n")

    ch = 0
    for prn in prns:
        # RF0
        out.append(f"Channel{ch}.signal={signal}")
        out.append(f"Channel{ch}.satellite={prn}")
        out.append(f"Channel{ch}.RF_channel_ID=0\n")
        ch += 1

        # RF1
        out.append(f"Channel{ch}.signal={signal}")
        out.append(f"Channel{ch}.satellite={prn}")
        out.append(f"Channel{ch}.RF_channel_ID=1\n")
        ch += 1

    return "\n".join(out)


def main():
    import sys

    if len(sys.argv) < 2:
        print("Usage: python3 generate_channels_block.py 3 5 7 10")
        return

    prns = [int(x) for x in sys.argv[1:]]

    result = generate_channels_block(prns)
    print(result)


if __name__ == "__main__":
    main()
