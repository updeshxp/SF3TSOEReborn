#!/usr/bin/env python3
"""
STFS extractor for Xbox 360 LIVE/PIRS/CON packages.
Ported from extract360.py by Rene Ladan to Python 3.

Original: Copyright (c) 2007, 2008, Rene Ladan, 2-clause BSD license.
Block reading algorithm from wxPirs.
"""
import hashlib
import os
import struct
import sys
import tempfile
import time
import xml.etree.ElementTree as ET


def _parse_stfs_header(input_path):
    """Parse STFS header and return (magic, data_start, hash_offset, ft_data)."""
    with open(input_path, 'rb') as f:
        fsize = os.path.getsize(input_path)
        magic = f.read(4)
        if magic not in (b'LIVE', b'PIRS', b'CON '):
            raise ValueError(f"Not a LIVE/PIRS/CON file: {magic}")
        if fsize < 0xD000:
            raise ValueError(f"File too small: {fsize} bytes (need at least 0xD000)")

        f.seek(0xC032)
        pathind = struct.unpack(">H", f.read(2))[0]
        if pathind == 0xFFFF:
            start = 0xC000
        else:
            start = 0xD000

        offset = 0x1000 if start == 0xC000 else 0x2000

        f.seek(start + 0x2F)
        firstclust = struct.unpack("<H", f.read(2))[0]
        max_ft_blocks = max(firstclust, 16)

        f.seek(start)
        ft_data = f.read(0x1000 * max_ft_blocks)

    return magic, start, offset, ft_data


def _read_entry_bytes(input_path, entry, start, offset):
    """Read a single file entry from the STFS package and return its bytes."""
    adstart = entry["startclust"] * 0x1000 + start
    remaining = entry["size"]
    data = bytearray()
    cur_clust = entry["startclust"]

    with open(input_path, 'rb') as f:
        while remaining > 0:
            realstart = adstart + get_cluster(cur_clust, offset)
            f.seek(realstart)
            chunk = f.read(min(0x1000, remaining))
            if not chunk:
                break
            data.extend(chunk)
            cur_clust += 1
            adstart += 0x1000
            remaining -= len(chunk)

    return bytes(data[:entry["size"]])


def _parse_arcade_xml(xml_bytes):
    """Parse ArcadeInfo.xml bytes and extract the game title."""
    try:
        root = ET.fromstring(xml_bytes)
        for elem in root.iter():
            if elem.tag.lower() in ("name", "title", "displayname"):
                text = (elem.text or "").strip()
                if text:
                    return text
        for elem in root.iter():
            if elem.tag.lower() == "arcadeinfo":
                for child in elem:
                    if child.tag.lower() == "name":
                        text = (child.text or "").strip()
                        if text:
                            return text
    except Exception:
        pass
    return None


def read_game_name(input_path):
    """Read the game display name from an STFS package.

    First tries to extract ArcadeInfo.xml from the package and parse it.
    Falls back to scanning the header for ASCII strings.
    """
    try:
        entries = list_live_pirs(input_path)
        arcade_entry = None
        for e in entries:
            if e["name"].lower() == "arcadeinfo.xml" and not e["is_dir"]:
                arcade_entry = e
                break
        if arcade_entry:
            magic, start, offset, _ = _parse_stfs_header(input_path)
            raw = _read_entry_bytes(input_path, arcade_entry, start, offset)
            name = _parse_arcade_xml(raw)
            if name:
                return name
    except Exception:
        pass

    with open(input_path, 'rb') as f:
        f.seek(0x300)
        data = f.read(0xD00 - 0x300)

    best = ""
    current = []
    for byte in data:
        if 32 <= byte < 127:
            current.append(chr(byte))
        else:
            if len(current) >= 5:
                word = "".join(current).strip()
                alpha = sum(1 for c in word if c.isalnum())
                if alpha >= 4 and len(word) > len(best):
                    best = word
            current = []
    if len(current) >= 5:
        word = "".join(current).strip()
        alpha = sum(1 for c in word if c.isalnum())
        if alpha >= 4 and len(word) > len(best):
            best = word

    return best if best else None


def get_cluster(startclust, offset):
    """Get the real starting cluster offset (from wxPirs algorithm)."""
    rst = 0
    while startclust >= 170:
        startclust //= 170
        rst += (startclust + 1) * offset
    return rst


def mstime(intime):
    """Convert Microsoft FAT time format to time tuple."""
    num_d = (intime & 0xFFFF0000) >> 16
    num_t = intime & 0x0000FFFF
    return ((num_d >> 9) + 1980, (num_d >> 5) & 0x0F, num_d & 0x1F,
            (num_t & 0xFFFF) >> 11, (num_t >> 5) & 0x3F, (num_t & 0x1F) * 2,
            0, 0, -1)


def list_live_pirs(input_path):
    """List all entries in an STFS package without extracting.

    Returns a list of dicts:
        {id, name, path, size, is_dir, is_contiguous, startclust, clustsize1}
    """
    magic, start, offset, ft_data = _parse_stfs_header(input_path)

    paths = {0xFFFF: ""}
    entries = []

    for i in range(len(ft_data) // 64):
        cur = ft_data[i * 64:(i + 1) * 64]
        namelen_flags = cur[40]
        name_len = namelen_flags & 0x3F
        is_dir = bool(namelen_flags & 0x80)
        is_contiguous = bool(namelen_flags & 0x40)

        if name_len == 0:
            break

        outname = cur[0:name_len].decode('ascii', errors='replace')

        if name_len < 1 or name_len > 40:
            continue

        clustsize1 = struct.unpack("<H", cur[41:43])[0] + (cur[43] << 16)
        startclust = struct.unpack("<H", cur[47:49])[0] + (cur[49] << 16)
        pathind = struct.unpack(">H", cur[50:52])[0]
        filelen = struct.unpack(">I", cur[52:56])[0]

        parent = paths.get(pathind, "")

        if is_dir:
            paths[i] = parent + outname + "/"
            entries.append({
                "id": i,
                "name": outname,
                "path": parent + outname + "/",
                "size": 0,
                "is_dir": True,
            })
        else:
            entries.append({
                "id": i,
                "name": outname,
                "path": parent + outname,
                "size": filelen,
                "is_dir": False,
                "is_contiguous": is_contiguous,
                "startclust": startclust,
                "clustsize1": clustsize1,
            })

    return entries


def extract_live_pirs(input_path, output_dir, selected_ids=None):
    """Extract files from a LIVE/PIRS STFS package.

    If selected_ids is provided (a set of entry IDs), only those entries
    are extracted. Entry IDs correspond to the 'id' field from list_live_pirs().
    """
    sys.stdout.reconfigure(encoding='utf-8')
    input_path = os.path.abspath(os.fspath(input_path))
    output_dir = os.path.abspath(os.fspath(output_dir))
    magic, start, offset, ft_data = _parse_stfs_header(input_path)

    print(f"Magic: {magic}")
    print(f"Data start: 0x{start:X}")
    print(f"Hash table offset: 0x{offset:X}")

    paths = {0xFFFF: ""}

    os.makedirs(output_dir, exist_ok=True)
    files_extracted = []

    for i in range(len(ft_data) // 64):
        cur = ft_data[i * 64:(i + 1) * 64]
        namelen_flags = cur[40]

        name_len = namelen_flags & 0x3F
        is_dir = bool(namelen_flags & 0x80)
        is_contiguous = bool(namelen_flags & 0x40)

        if name_len == 0:
            break

        outname = cur[0:name_len].decode('ascii', errors='replace')

        clustsize1 = struct.unpack("<H", cur[41:43])[0] + (cur[43] << 16)
        clustsize2 = struct.unpack("<H", cur[44:46])[0] + (cur[46] << 16)
        startclust = struct.unpack("<H", cur[47:49])[0] + (cur[49] << 16)
        pathind = struct.unpack(">H", cur[50:52])[0]
        filelen = struct.unpack(">I", cur[52:56])[0]
        dati1 = struct.unpack(">I", cur[56:60])[0]
        dati2 = struct.unpack(">I", cur[60:64])[0]

        type_str = "DIR " if is_dir else "FILE"
        contig = " [contiguous]" if is_contiguous else ""
        print(f"  [{type_str}] {outname:<40s} {filelen:>12d} bytes  start_block={startclust}  blocks={clustsize1}{contig}  path={pathind}")

        if name_len < 1 or name_len > 40:
            print(f"    WARNING: Name length {name_len} out of range, skipping")
            continue

        if clustsize1 != clustsize2:
            print(f"    WARNING: Cluster sizes don't match ({clustsize1} != {clustsize2})")

        if is_dir:
            paths[i] = paths.get(pathind, "") + outname + "/"
            full_dir = os.path.join(output_dir, paths[i])
            os.makedirs(full_dir, exist_ok=True)
            print(f"    -> Created directory: {paths[i]}")
        else:
            if selected_ids is not None and i not in selected_ids:
                print(f"    -> Skipped (not selected)")
                continue

            parent = paths.get(pathind, "")
            out_path = os.path.join(output_dir, parent, outname)
            os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)

            adstart = startclust * 0x1000 + start
            remaining = filelen
            file_data = bytearray()
            cur_clust = startclust

            with open(input_path, 'rb') as infile:
                while remaining > 0:
                    realstart = adstart + get_cluster(cur_clust, offset)
                    infile.seek(realstart)
                    chunk = infile.read(min(0x1000, remaining))
                    if not chunk:
                        print(f"    WARNING: Read failed at offset 0x{realstart:X}")
                        break
                    file_data.extend(chunk)
                    cur_clust += 1
                    adstart += 0x1000
                    remaining -= len(chunk)

            with open(out_path, 'wb') as outfile:
                outfile.write(bytes(file_data[:filelen]))

            file_magic = bytes(file_data[:4]) if len(file_data) >= 4 else b''
            magic_str = ""
            if file_magic == b'XEX2':
                magic_str = " >> XEX2 executable!"
            elif file_magic == b'XEX1':
                magic_str = " >> XEX1 executable!"
            elif file_magic == b'\x89PNG':
                magic_str = " >> PNG image"

            print(f"    -> Extracted: {out_path} ({filelen} bytes){magic_str}")
            files_extracted.append((outname, out_path, filelen, file_magic))

    print(f"\nDone! Extracted {len(files_extracted)} file(s) to {output_dir}")

    return files_extracted


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <stfs_package> [output_dir]")
        print("  Extracts files from a LIVE/PIRS/CON Xbox 360 STFS package.")
        sys.exit(1)

    input_path = os.path.abspath(sys.argv[1])

    if len(sys.argv) < 3:
        output_dir = os.path.abspath(os.path.join(os.path.dirname(input_path), '../assets'))
    else:
        output_dir = os.path.abspath(sys.argv[2])

    print(f"Input:  {input_path}")
    print(f"Output: {output_dir}")
    print()

    extract_live_pirs(input_path, output_dir)
