#!/usr/bin/env python3
"""
Test script to verify Hangar API responses and data structure
"""
import requests
import json
from pprint import pprint

BASE_URL = "https://hangar.papermc.io/api/v1"

def test_search():
    """Test plugin search endpoint"""
    print("=" * 80)
    print("Testing /projects (search)")
    print("=" * 80)
    
    url = f"{BASE_URL}/projects"
    params = {
        "limit": 3,
        "offset": 0,
        "platform": "PAPER"
    }
    
    response = requests.get(url, params=params)
    print(f"URL: {response.url}")
    print(f"Status: {response.status_code}\n")
    
    if response.status_code == 200:
        data = response.json()
        print(f"Response structure: {list(data.keys())}")
        
        if "result" in data:
            projects = data["result"]
            print(f"Found {len(projects)} projects\n")
            
            for i, proj in enumerate(projects[:2], 1):
                print(f"\n--- Project {i} ---")
                print(f"Name: {proj.get('name')}")
                print(f"Slug: {proj.get('slug')}")
                print(f"Namespace: {proj.get('namespace')}")
                print(f"Description: {proj.get('description', '')[:100]}...")
                print(f"AvatarUrl: {proj.get('avatarUrl')}")
                print(f"Stats: {proj.get('stats')}")
                
                # Save first project for detailed inspection
                if i == 1:
                    owner = proj.get('namespace', {}).get('owner')
                    slug = proj.get('namespace', {}).get('slug')
                    return owner, slug
    else:
        print(f"Error: {response.text}")
        return None, None

def test_project_info(owner, slug):
    """Test project info endpoint"""
    print("\n" + "=" * 80)
    print(f"Testing /projects/{owner}/{slug} (project info)")
    print("=" * 80)
    
    url = f"{BASE_URL}/projects/{owner}/{slug}"
    response = requests.get(url)
    print(f"URL: {response.url}")
    print(f"Status: {response.status_code}\n")
    
    if response.status_code == 200:
        data = response.json()
        print(f"Response keys: {list(data.keys())}")
        print(f"\nName: {data.get('name')}")
        print(f"Description: {data.get('description', '')[:200]}...")
        print(f"AvatarUrl: {data.get('avatarUrl')}")
        print(f"Namespace: {data.get('namespace')}")
        print(f"Settings: {data.get('settings')}")
        print(f"Stats: {data.get('stats')}")
    else:
        print(f"Error: {response.text}")

def test_versions(owner, slug):
    """Test versions endpoint"""
    print("\n" + "=" * 80)
    print(f"Testing /projects/{owner}/{slug}/versions")
    print("=" * 80)
    
    url = f"{BASE_URL}/projects/{owner}/{slug}/versions"
    params = {
        "limit": 5,
        "offset": 0
    }
    
    response = requests.get(url, params=params)
    print(f"URL: {response.url}")
    print(f"Status: {response.status_code}\n")
    
    if response.status_code == 200:
        data = response.json()
        print(f"Response structure: {list(data.keys())}")
        
        if "result" in data:
            versions = data["result"]
            print(f"Found {len(versions)} versions\n")
            
            for i, ver in enumerate(versions[:3], 1):
                print(f"\n--- Version {i} ---")
                print(f"Name: {ver.get('name')}")
                print(f"Version: {ver.get('version')}")
                print(f"Channel: {ver.get('channel')}")
                print(f"Downloads: {list(ver.get('downloads', {}).keys())}")
                
                # Check first download platform
                downloads = ver.get('downloads', {})
                if downloads:
                    first_platform = list(downloads.keys())[0]
                    download_info = downloads[first_platform]
                    print(f"\nPlatform {first_platform}:")
                    print(f"  downloadUrl: {download_info.get('downloadUrl')}")
                    print(f"  externalUrl: {download_info.get('externalUrl')}")
                    print(f"  fileInfo: {download_info.get('fileInfo')}")
                
                print(f"PlatformDependencies: {ver.get('platformDependencies')}")
                print(f"ID: {ver.get('id')}")
                
                # Detailed inspection of first version
                if i == 1:
                    print(f"\n--- Full Version Object (first 500 chars) ---")
                    print(json.dumps(ver, indent=2)[:500])
        else:
            print(f"Unexpected response structure:")
            pprint(data)
    else:
        print(f"Error: {response.text}")

def main():
    print("Hangar API Test Script")
    print("Testing plugin search, info, and versions endpoints\n")
    
    # Test search and get first project
    owner, slug = test_search()
    
    if owner and slug:
        # Test project info
        test_project_info(owner, slug)
        
        # Test versions
        test_versions(owner, slug)
    else:
        print("\nFailed to get project from search, skipping detailed tests")

if __name__ == "__main__":
    main()
