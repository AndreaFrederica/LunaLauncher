# BMCLAPI Integration & Meta Structure Analysis

This document summarizes the differences between Prism Launcher's metadata format and the Mojang/BMCLAPI format, and outlines the strategy for implementing BMCLAPI mirror support.

## 1. Metadata Format Comparison

### Prism Launcher Meta (`index.json`)
*   **Format**: Custom JSON structure designed for component-based architecture.
*   **Key Features**:
    *   **Explicit Dependency Management**: Top-level index files declare dependencies (e.g., `requires: net.minecraft`, `org.lwjgl3`). This allows the launcher to resolve the full dependency graph *before* downloading version-specific files.
    *   **Component System**: Treats all parts (Minecraft, Fabric, Forge, LWJGL) as equal components that can be mixed and matched.
    *   **Rich Metadata**: Includes recommendation levels (`recommended: true`), upstream info, and conflict definitions.

### BMCLAPI / Mojang (`version_manifest_v2.json`)
*   **Format**: Standard Mojang JSON format.
*   **Key Features**:
    *   **Monolithic Structure**: Lists available Minecraft versions with basic metadata (ID, type, release time, URL).
    *   **Implicit Dependencies**: Dependencies (libraries, assets) are hidden inside individual version JSONs. You must download the specific version JSON to know what libraries it needs.
    *   **No Component Data**: Does not contain information about mod loaders (Fabric/Forge) or system libraries (LWJGL) at the manifest level.

### Impact on Mirroring
*   **Incompatibility**: Prism Launcher cannot directly consume BMCLAPI's `version_manifest.json` because it lacks the critical dependency information required by Prism's `VersionList` and `Component` system.
*   **Missing Data**: Replacing the Prism Meta URL with BMCLAPI directly would break the launcher's ability to resolve library versions and loader compatibility.

## 2. Implementation Strategy

To support BMCLAPI while maintaining Prism's functionality, we adopted a **Client-Side Artifact Rewriting** strategy.

### Implemented Solution (Client-Side Rewrite)
Instead of trying to force Prism to read Mojang-formatted manifests, we keep using the Prism Meta format for the *index* but redirect all *heavy* downloads to BMCLAPI.

1.  **Library Downloads** (`launcher/minecraft/Library.cpp`):
    *   Intercepts download requests for libraries.
    *   Rewrites URLs from `libraries.minecraft.net`, `maven.fabricmc.net`, etc., to `bmclapi2.bangbang93.com/maven`.
    *   Handles path adjustments for different upstream sources.

2.  **Asset Downloads** (`launcher/minecraft/update/AssetUpdateTask.cpp`):
    *   Intercepts asset index and object downloads.
    *   Rewrites `resources.download.minecraft.net` to BMCLAPI's asset mirror.

3.  **Meta URL Rewriting** (`launcher/minecraft/Library.cpp`):
    *   Even though we use Prism's index, the index often points to Mojang URLs for specific version files (e.g., `piston-meta.mojang.com`).
    *   We implemented logic to rewrite these specific Mojang metadata URLs to BMCLAPI on the fly.

### Alternative Approaches (Discarded)

*   **Server-Side Mirror (Scheme A)**:
    *   *Concept*: Host a mirror that replicates `meta.prismlauncher.org` but replaces all internal URLs with BMCLAPI.
    *   *Pros*: Zero client code changes required for URL structure.
    *   *Cons*: Requires server infrastructure and maintenance.

*   **Client-Side Adapter (Scheme B)**:
    *   *Concept*: Write a `MojangMetaProvider` class in C++ to parse `version_manifest.json` and convert it to Prism's `VersionList` in memory.
    *   *Pros*: Direct usage of BMCLAPI/Mojang manifests.
    *   *Cons*: High complexity. Requires "guessing" or hardcoding dependencies (e.g., "If version > 1.19, use LWJGL 3") or performing expensive network lookups during version listing.

## 3. Summary of Changes

We have successfully implemented the **Client-Side Artifact Rewriting** solution.

*   **Codebase**:
    *   Modified `Library.cpp` to handle library and meta URL rewriting.
    *   Modified `AssetUpdateTask.cpp` for asset mirrors.
    *   Updated `MirrorDownload.cpp` with BMCLAPI definitions.
*   **Testing**:
    *   Added `BMCLAPITest.cpp` (Unit Test): Verifies URL rewriting logic for various scenarios (Forge, Fabric, Mojang).
    *   Added `BMCLAPICompareTest.cpp` (Integration Test): Verifies that BMCLAPI's content matches Mojang's official content, ensuring safety.

This approach ensures users get the speed benefits of BMCLAPI for 99% of data (game files, libraries, assets) while maintaining the stability and correctness of Prism's component system.
