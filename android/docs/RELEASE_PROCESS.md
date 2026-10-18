# Version and GitHub update policy

Owner requested every delivered app update to be reflected on GitHub with an explanation of changes.

1. Increment Android versionCode monotonically and update versionName for each delivered build (not for every intermediate edit).
2. Add a dated CHANGELOG entry describing user-visible changes, relevant validation and remaining limitations. Keep prepared/config-gated features distinct from live services.
3. Build/test relevant changed behavior. Install on the S22 with `adb install --user 0 -r <apk>` only (never omit --user on this Samsung Dual App device); then check user95 remains uninstalled. Install on the S22 when available and record actual evidence. Preserve vehicle data; keep raw phone databases, screenshots, logs, tokens/PIN and local settings outside the public repository.
4. Copy current source/docs and only curated aggregate test results into the public android/ tree. Review diff and secrets/artifact exclusions.
5. Commit and push, then tag a verified build as android-v<versionName>. Do not rewrite existing tags. Verify remote HEAD/tag before reporting delivery.
6. Refresh local APK/source archive and delivery checksum file. Unsigned release build success is not Google Play publication approval.
