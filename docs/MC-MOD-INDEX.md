# MCMod index

Luna uses MCMod as a read-only metadata index. It does not scrape MCMod at
runtime and it does not use MCMod as a binary download provider. Modrinth and
CurseForge remain responsible for search, versions, dependencies and files.

The runtime reads `launcher/exresources/mcmod/mcmod.csv` from the launcher
resource directories. The data has its own attribution and terms in
`launcher/exresources/mcmod/MC-MOD-LICENSE.txt`; this is separate from the Luna
Launcher source-code license. The current dataset is imported from this pinned
HMCL revision:

```text
https://github.com/HMCL-dev/HMCL/blob/bdbf5773a85718cfdbfc40cbed5ff6c92bf69cbc/HMCL/src/main/resources/assets/addon/mod.csv
```

When updating the dataset, verify the source revision and its attribution
before replacing the file. For an independently authorized JSON export, the
converter remains available:

The imported file currently contains 31,626 records and has SHA-256
`e3a03ba1a7f35c8d8035e99c380a68c9bff6bdf1d252f583506514f268a7396f`.

```text
python tools/mcmod/update_mcmod_data.py path/to/authorized-export.json launcher/exresources/mcmod/mcmod.csv
```

The generated fields are compatible with the HMCL index shape:

```text
mcmod_id,curseforge_id,modrinth_id,main_name,secondary_name,abbreviation,mod_ids
```

Only identifiers, names and links should be included. Do not add MCMod article
descriptions or images without checking their terms. Keep the export provenance
and generation date with the release artifacts, and ship
`MC-MOD-LICENSE.txt` alongside the generated data.

At runtime the index is used in three places:

* installed mods are matched by `mod_id` first, then by the local mod name;
* the classic resource information panel links a match to the MCMod class page
  and offers an MCMod search link when there is no match;
* a Chinese mod search term is translated through the matched record's secondary
  name (then abbreviation) before the same query is sent to Modrinth or
  CurseForge. Non-Chinese and unmatched queries are left unchanged.

This keeps MCMod as a metadata and discovery source. The provider APIs still
perform remote searches and all downloads.
