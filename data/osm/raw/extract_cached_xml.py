#!/usr/bin/env python3
"""
GoRide - Phase 2A.1 Recovery: Extract Cached Overpass XML
Transfers the genuine 2.85 MB OpenStreetMap XML response cached during Overpass API query
to the project repository raw data directory: data/osm/raw/hcmc_road_network.osm.xml
"""

import os
import sys
from pathlib import Path

CACHED_XML_LOG = Path(
    r"C:\Users\HP\.gemini\antigravity\brain\ebbbea0e-3aef-4f2a-832c-834826ee85dc\.system_generated\steps\576\content.md"
)

def extract_xml(target_path: Path) -> bool:
    target_path.parent.mkdir(parents=True, exist_ok=True)
    
    print(f"[INFO] Checking cached XML log at: {CACHED_XML_LOG}")
    if not CACHED_XML_LOG.exists():
        print(f"[WARN] Cache file not found. Falling back to fresh Overpass API download...")
        from download_osm import download_osm_data
        return download_osm_data(str(target_path))

    print(f"[INFO] Reading cached Overpass XML ({CACHED_XML_LOG.stat().st_size:,} bytes)...")
    
    # Lines 1-8 are Markdown HTTP header wrapper added by fetch tool.
    # Line 9 starts: <?xml version="1.0" encoding="UTF-8"?>
    xml_lines = []
    found_xml_start = False
    
    with open(CACHED_XML_LOG, "r", encoding="utf-8", errors="replace") as fin:
        for line in fin:
            if not found_xml_start:
                if line.strip().startswith("<?xml"):
                    found_xml_start = True
                    xml_lines.append(line)
            else:
                xml_lines.append(line)
                if "</osm>" in line:
                    break

    if not found_xml_start or not xml_lines:
        print("[ERROR] Could not locate <?xml start tag in cache file.")
        return False

    with open(target_path, "w", encoding="utf-8") as fout:
        fout.writelines(xml_lines)

    out_size = target_path.stat().st_size
    print(f"[SUCCESS] Successfully extracted genuine OSM XML to: {target_path}")
    print(f"          - Total extracted lines: {len(xml_lines):,}")
    print(f"          - File size: {out_size:,} bytes ({out_size / (1024*1024):.2f} MB)")
    return True

if __name__ == "__main__":
    script_dir = Path(__file__).resolve().parent
    default_target = script_dir / "hcmc_road_network.osm.xml"
    out_file = Path(sys.argv[1]) if len(sys.argv) > 1 else default_target
    
    print("=" * 70)
    print("GoRide - OSM XML Cache Extraction Utility")
    print(f"Target: {out_file}")
    print("=" * 70)
    
    success = extract_xml(out_file)
    sys.exit(0 if success else 1)
