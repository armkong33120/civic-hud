## Implementation update — 18 October 2026

The offline Android MVP is implemented under `android/`. This document is historical phase2/cloud planning, not the current code contract. See `../android/README.md` and `../android/MORNING_TEST_GUIDE.md`. Current distance source is speed010Donly; cloud/login remain excluded.

# Android product/system design — draft, not implemented

## 1. Requirements

Free accounts for about20 users; each owns a vehicle and OBD adapter. Maintain useful dashboard mileage and countdowns while driving; record service mileage/time plus optional location, shop/mechanic, phone, cost and notes. History belongs to its owner. Work offline after initial sign-in and sync later. Record capability and data confidence instead of claiming all adapters provide lifetime odometer.

Primary screens: sign-in, vehicle/adapter onboarding and dashboard baseline, dashboard, service detail/confirmation, history, settings/sync status. Thai-friendly UI; no mandatory location/contact fields. Dark/light themes and approved urgency bars. Android wireframes are not yet made; mockups in repo are ESP32 references.

## 2. Capacity estimate (planning assumptions)

20 users × up to5 vehicles =100 vehicles; 30 maintenance definitions pervehicle =3,000 rows. Assume2 trip summaries/user/day =14,600 rows/year; 2 service records/user/month =480/year. At2KB structured payload/row this is roughly30MB/year for trip/service payload only, before DB indexes, backups and logs. Photos/raw telemetry can dominate and are excluded from MVP assumptions. Cloud sizing/cost must be checked against actual Azure offer later.

Store local distance checkpoints for crash recovery, sync trip aggregates/endpoints and service events. Do not upload every 1-second speed sample indefinitely. Suggested periodic checkpoint sync only when online, batched; final interval/retention are open decisions.

## 3. Proposed data model

All IDs UUIDs; integer meters for persisted distance, UTC timestamps with optional timezone context. SQL schema/migrations not implemented.

- User: auth_subject, created_at. Credential handling delegated to selected auth provider; no passwords in app database.
- Vehicle: id, owner_id, make/model/year, display_name, version.
- AdapterProfile: vehicle_id, transport Classic/BLE, supported protocol/PIDs, local connection identifier; do not treat ELM version string as capability proof.
- OdometerBaseline: vehicle_id, dashboard_meters, ecu_counter, captured_at, source, calibration revision.
- DistanceCheckpoint/Trip: id, vehicle_id, device_id, sequence, source, raw_counter, integrated_meters, quality/gap state, started_at/ended_at.
- MaintenanceItem: vehicle_id, title, interval_meters/interval_days, last_service event reference, visible, version.
- ServiceRecord: id, vehicle_id, maintenance_item_id, performed_at, odometer_meters, optional latitude/longitude, shop_name, mechanic_name, phone, cost/currency, notes, version/deleted_at.
- MileageAdjustment: old/new meters, reason, actor/time, revision. Keep corrections traceable; don't overwrite raw observed distance as if from ECU.
- SyncOutbox: operation_id, entity/version, payload, retry state. SyncCursor per account/device.
- ReminderState: maintenance cycle + threshold + last_notified; avoid repeating the same notification every OBD packet.

## 4. High-level architecture

```text
OBD adapter → Bluetooth transport → ELM/vehicle decoder → distance engine
                                                         ↓
                                              local database (Room)
                                               ↙                 ↘
                                    UI / reminder rules      persistent sync queue
                                                                    ↕ HTTPS
                                                         auth + application API
                                                                    ↓
                                                        owner-controlled database
```

Suggested Android implementation: Kotlin + Compose + Room. Background connected-device service only while maintaining vehicle connection, user-visible notification and stop control. Exact lifecycle/permissions need implementation against chosen Android target. Cloud vendor and deployment topology remain open; one application backend and one database are sufficient as an initial design for this user count.

Server verifies authenticated ownership for every object read/write, not just client-supplied owner_id. Optional location only requested when saving service, not continuous GPS tracking. Keep secrets out of APK; export/delete user data and backup restore procedure must exist before broader release.

## 5. Proposed API contracts (not running endpoints)

- GET /v1/me
- GET/POST /v1/vehicles
- GET/PATCH /v1/vehicles/{id} (expected revision)
- POST /v1/vehicles/{id}/trip-events (unique event_id/device sequence)
- POST /v1/vehicles/{id}/mileage-adjustments
- GET/PUT /v1/vehicles/{id}/maintenance/{itemId}
- POST /v1/vehicles/{id}/service-records (idempotency key)
- GET /v1/sync?cursor=... → changed entities, tombstones and next cursor
- POST /v1/sync → operation IDs, expected revisions, per-operation accepted/conflict results

Service save + next-cycle reset in one local transaction and corresponding server transaction. Idempotency prevents duplicate records after retries. Use revision conflicts for maintenance edits; do not silently last-write-win mileage counters.

## 6. Deep dive and trade-offs

Distance strategy per vehicle: confirmed absolute odometer if actually supported; otherwise dashboard baseline + ECU delta counter; otherwise speed integration with bounded valid sampling gaps. PID0131 is distance since DTC clear and can reset/wrap. Handle stale frames, malformed responses, ECU sleep, NO DATA, reconnection and transport framing independently.

When connection is missing, mark the gap visibly. Do not integrate a last known speed for an unbounded period. An ECU cumulative counter may recover distance later only after continuity is established; dashboard correction remains available. Avoid double counting by tracking accepted sample/checkpoint and distance source transitions.

For MVP propose one active recording phone per vehicle, with explicit handover. Do not sum overlapping trips from two phones. Define server recording-session ownership and conflict handling before enabling multiple active devices.

Notification decisions can be made locally without internet. Sync account history/cloud backup separately from real-time distance calculation. Validate screen-off behavior, process death, reconnect, ECU reset, DB migration and sync retries; test two users cannot access each other's records.

## References for future implementation

- https://developer.android.com/topic/architecture/data-layer/offline-first
- https://developer.android.com/develop/background-work/services/fgs/service-types
- https://developer.android.com/develop/connectivity/bluetooth/bt-permissions
- https://elmelectronics.com/wp-content/uploads/2020/05/ELM327DSL.pdf

These are design pointers, not proof a particular user's phone/adapter is compatible.
