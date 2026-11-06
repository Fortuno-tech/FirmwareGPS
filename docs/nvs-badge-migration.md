# NVS badge whitelist migration

The deployed legacy blob format is a `uint32_t` badge count followed by two
32-byte badge digests. The versioned target format is `magic` (`0x46525431`),
`version` (`1`), a `uint16_t` count, then the same two digest slots. Historical
firmware source used this legacy layout with the 32-byte `SecurityService`
badge hash.

Migration is controlled by `FORTICO_ENABLE_NVS_MIGRATION`, default `0`
(disabled). It can also be explicitly enabled by the `StorageService`
constructor for controlled rollout and software tests. When enabled, startup
promotes the legacy blob to version 1 and removes the known shared badge
`91:51:E2:06`; the update is idempotent if startup runs again. Unknown,
malformed versioned data or an invalid legacy count fails storage startup
closed.

When disabled, the legacy blob remains readable/writable in its original
format. The known shared badge is still removed on startup, preserving the
existing firmware behavior; that cleanup is kept separate from the optional
format migration. Enabling the format flag requires a staged rollout and a
recovery plan. The migration does not add a credential, secret, or default
badge.

Native tests cover legacy and versioned blobs, repeated migration, an empty
NVS namespace, and the disabled-by-default format behavior. They do not prove
compatibility with every field-provisioned NVS image; collect representative
backups before enabling migration on deployed vehicles.
