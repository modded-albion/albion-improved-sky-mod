#!/usr/bin/env python3
"""Convert the Yale Bright Star Catalogue (BSC5) to the star table of the sky mod.

Usage:
    python make_stars.py [--catalog D:/masters_of_albion/cache/sky/bsc5/catalog.gz] [--output stars_bsc5.bin]
    python make_stars.py --self-test

The source is the file `catalog.gz` of CDS/VizieR V/50 (Hoffleit and Warren 1991):
https://cdsarc.cds.unistra.fr/ftp/V/50/catalog.gz

The star table (little endian):
    8 bytes  "MOASTARS"
    uint32   version (1)
    uint32   count of stars
    count records of 12 bytes, brightest first:
        float32  right ascension J2000, degrees
        float32  declination J2000, degrees
        int16    V magnitude times 100
        int16    B-V colour index times 1000, or -32768 if the catalogue has no B-V
The catalogue entries with no J2000 position or no V magnitude (novae and objects that are not stars) are not in the
table.
"""

import argparse
import gzip
import pathlib
import struct
import sys

k_Magic = b"MOASTARS"
k_Version = 1
k_NoColour = -32768
k_DefaultCatalog = pathlib.Path(r"D:\masters_of_albion\cache\sky\bsc5\catalog.gz")
k_DefaultOutput = pathlib.Path(__file__).resolve().parent / "stars_bsc5.bin"


def field(line: str, first: int, last: int) -> str:
    """Return the bytes `first` to `last` of a record (1-based, as in the ReadMe of the catalogue)."""
    return line[first - 1:last].strip()


def parse_record(line: str):
    """Return (right ascension, declination, V, B-V or None) in degrees and magnitudes, or None for no star."""
    ra_hours, ra_minutes, ra_seconds = field(line, 76, 77), field(line, 78, 79), field(line, 80, 83)
    sign, dec_degrees, dec_minutes, dec_seconds = field(line, 84, 84), field(line, 85, 86), field(line, 87, 88), field(line, 89, 90)
    vmag = field(line, 103, 107)
    if not ra_hours or not dec_degrees or not vmag:
        return None
    right_ascension = (int(ra_hours) + int(ra_minutes) / 60.0 + float(ra_seconds) / 3600.0) * 15.0
    declination = int(dec_degrees) + int(dec_minutes) / 60.0 + int(dec_seconds) / 3600.0
    if sign == "-":
        declination = -declination
    colour = field(line, 110, 114)
    return right_ascension, declination, float(vmag), float(colour) if colour else None


def convert(lines) -> bytes:
    stars = [star for star in (parse_record(line) for line in lines) if star is not None]
    stars.sort(key=lambda star: star[2])
    out = bytearray(k_Magic + struct.pack("<II", k_Version, len(stars)))
    for right_ascension, declination, vmag, colour in stars:
        packed_colour = k_NoColour if colour is None else round(colour * 1000.0)
        out += struct.pack("<ffhh", right_ascension, declination, round(vmag * 100.0), packed_colour)
    return bytes(out)


def read_table(data: bytes):
    """Read a star table back: a list of (right ascension, declination, V, B-V or None)."""
    if data[:8] != k_Magic:
        raise ValueError("not a star table")
    version, count = struct.unpack_from("<II", data, 8)
    if version != k_Version or len(data) != 16 + 12 * count:
        raise ValueError("wrong version or size")
    stars = []
    for index in range(count):
        right_ascension, declination, vmag, colour = struct.unpack_from("<ffhh", data, 16 + 12 * index)
        stars.append((right_ascension, declination, vmag / 100.0, None if colour == k_NoColour else colour / 1000.0))
    return stars


def self_test() -> int:
    # Records in the layout of the catalogue: only the bytes that the script reads have values.
    def record(ra: str, dec: str, vmag: str, colour: str) -> str:
        line = [" "] * 197
        line[75:90] = list(ra + dec)
        line[102:107] = list(vmag.rjust(5))
        line[109:114] = list(colour.rjust(5))
        return "".join(line)

    sirius = record("064508.9", "-164258", "-1.46", " 0.00")
    polaris = record("023148.7", "+891551", " 2.02", " 0.60")
    nova = record("      ", "       ", "     ", "     ")
    no_colour = record("120000.0", "+000000", " 6.00", "     ")
    table = read_table(convert([polaris, nova, no_colour, sirius]))
    assert len(table) == 3, table
    ra, dec, vmag, colour = table[0]
    assert abs(ra - 101.287) < 1e-3 and abs(dec + 16.716) < 1e-3, table[0]
    assert vmag == -1.46 and colour == 0.0, table[0]
    assert abs(table[1][1] - 89.264) < 1e-3 and table[1][2] == 2.02 and table[1][3] == 0.6, table[1]
    assert table[2][3] is None and table[2][0] == 180.0, table[2]
    print("self-test: OK")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--catalog", type=pathlib.Path, default=k_DefaultCatalog)
    parser.add_argument("--output", type=pathlib.Path, default=k_DefaultOutput)
    parser.add_argument("--self-test", action="store_true", help="run the tests on made records and stop")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    with gzip.open(args.catalog, "rt", encoding="ascii") as source:
        data = convert(source.read().splitlines())
    args.output.write_bytes(data)
    stars = read_table(data)
    print(f"wrote {args.output} ({len(data)} bytes, {len(stars)} stars, faintest V {stars[-1][2]:.2f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
