# Export verification — 2026-10-14

- Copied production sketch and TFT setup; no production behavior changed. Hardware was not flashed or connected during export.
- Rebuilt copied sketch with esp32 core3.3.10: program2,090,420bytes; globals71,188bytes, matching previous report. See evidence/export-build.log.
- Ran focused bar regression against copied code:24 distinct RGB565 shades, shade order,501/500km threshold, overdue,time-only,reset passed.
- Ran display transport regression from copied production body with only ARDUINO wrapper removed: cached unchanged display, clipped header, card/slider-only updates, page/modal transitions and bar-only blinking passed. Generated host header is explicitly marked.
- Looked for common credential/token/private-key patterns in tracked text; no matches found. This is a targeted inspection, not a formal security audit.
- No NVS/flash dumps, account tokens or Azure account information included. Adapter MACs in text redacted; generic pairing PIN remains firmware default.
- Firmware source files match the original working sketch byte-for-byte except deliberate MAC redaction where applicable; manifests capture exported bytes.
- Historic reports, tests and agent notes are archived as evidence, not treated as current task instructions or universal hardware guarantees.
- Android and Azure remain design/deferred. Notion article is prepared locally; publication status must be checked in the agent's final report.
