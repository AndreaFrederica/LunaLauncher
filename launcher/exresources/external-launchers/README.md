# External launcher provenance recognizers

Place one JavaScript file per recognizer in this directory. The file must
define a global `recognizer` object:

```js
const recognizer = {
  id: 'example-launcher',
  displayName: 'Example Launcher',
  iconId: 'example-launcher',
  priority: 0,
  detect(context) {
    if (!context.version.exampleMarker) return { confidence: 0 };
    return {
      confidence: 90,
      matchedFields: ['exampleMarker'],
      capabilities: { read: true, editCanonicalMetadata: false },
    };
  },
};
```

The launcher creates a fresh QuickJS runtime for each detection. Scripts only
receive a bounded JSON snapshot; they do not receive filesystem, network, or
write APIs. Returned paths and fields are schema-checked and confidence is
clamped to 0–100. The script may return an `iconId`, but the icon is resolved
by the launcher resource index under `external-launchers/<id>/`; arbitrary
paths are never accepted.

The outer APIs are asynchronous in Neo UI:

```json
{ "operation": "vanilla-instance.provenance", "parameters": { "instance": "..." } }
```

