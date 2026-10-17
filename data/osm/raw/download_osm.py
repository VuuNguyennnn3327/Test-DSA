#!/usr/bin/env python3
"""
GoRide - Phase 2A.1: OSM Road Network Downloader
Downloads genuine road network XML data for Ho Chi Minh City from OpenStreetMap Overpass API.

Target Bounding Box: [10.760, 106.650, 10.825, 106.730]
Coverage: Ben Thanh Market, Notre Dame Cathedral, Landmark 81, Tan Son Nhat Airport, Bach Khoa University.
"""

import os
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

# Overpass API mirrors
OVERPASS_MIRRORS = [
    "https://overpass.openstreetmap.fr/api/interpreter",
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
]

OVERPASS_QUERY = """[timeout:90];
(
  way["highway"~"^(motorway|trunk|primary|secondary|tertiary|motorway_link|trunk_link|primary_link|secondary_link|tertiary_link|unclassified)$"](10.760,106.650,10.825,106.730);
);
out body;
>;
out skel qt;
"""

def download_osm_data(output_file: str) -> bool:
    """Download OSM data with mirror fallback and retry logic."""
    target_path = Path(output_file)
    target_path.parent.mkdir(parents=True, exist_ok=True)
    
    encoded_query = urllib.parse.quote(OVERPASS_QUERY)
    user_agent = "GoRide-DSA-Project/1.0 (DSA Course Project; University of Technology; Contact: dsa@goride.local)"
    
    for mirror in OVERPASS_MIRRORS:
        url = f"{mirror}?data={encoded_query}"
        print(f"[INFO] Attempting download from mirror: {mirror}")
        try:
            req = urllib.request.Request(
                url,
                headers={
                    "User-Agent": user_agent,
                    "Accept": "application/xml, text/xml, */*",
                }
            )
            
            start_time = time.time()
            with urllib.request.urlopen(req, timeout=120) as response:
                if response.status != 200:
                    print(f"[WARN] HTTP {response.status} from {mirror}")
                    continue
                
                content = response.read()
                elapsed = time.time() - start_time
                
                # Validation
                text = content.decode("utf-8", errors="replace")
                if "<osm" not in text or "</osm>" not in text:
                    print(f"[WARN] Incomplete OSM XML received from {mirror}")
                    continue
                
                if "<way" not in text or "<node" not in text:
                    print(f"[WARN] Response missing way/node elements from {mirror}")
                    continue
                
                with open(target_path, "wb") as f:
                    f.write(content)
                
                file_size_mb = len(content) / (1024 * 1024)
                print(f"[SUCCESS] Downloaded {file_size_mb:.2f} MB in {elapsed:.2f}s -> {target_path}")
                return True
                
        except Exception as e:
            print(f"[WARN] Error connecting to {mirror}: {e}")
            time.sleep(2)
            continue
            
    print("[ERROR] All Overpass API mirrors failed.")
    return False

if __name__ == "__main__":
    script_dir = Path(__file__).resolve().parent
    default_output = script_dir / "hcmc_road_network.osm.xml"
    output_path = sys.argv[1] if len(sys.argv) > 1 else str(default_output)
    
    print("=" * 60)
    print("GoRide - OSM Road Network Download Script")
    print(f"Output Target: {output_path}")
    print("=" * 60)
    
    success = download_osm_data(output_path)
    sys.exit(0 if success else 1)
