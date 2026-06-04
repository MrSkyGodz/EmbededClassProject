# IMU Reference Control ICD

This feature controls a two-servo gimbal from an IMU-referenced azimuth/elevation target. The wire format is unchanged: the upper system still sends azimuth/elevation floats, and firmware converts them into servo position commands.

## Coordinate Mapping

- `targetAzimuthDeg` and `targetElevationDeg` are IMU references, not raw motor commands.
- Azimuth feedback is BNO055 telemetry `eulerX`.
- Elevation feedback is BNO055 telemetry `eulerZ`.
- Motor1 drives azimuth and remains physically inverted in firmware.
- Motor2 drives elevation directly.
- Both servos are treated as `0..180` position actuators.
- The controller does not use encoder feedback. It uses BNO055 feedback for actual look direction and the last sent motor command as the motor-position estimate.

## ICD Types

All ICD payloads use little-endian floats and avoid C struct padding on the wire.

| ICD Type | Name | Direction | Payload |
| --- | --- | --- | --- |
| 3 | `imuReferenceControl` | command | `float targetAzimuthDeg`, `float targetElevationDeg`, `uint8 enable`, `uint8 frameMode` |
| 4 | `imuReferenceTuning` | command | `float azimuthKp`, `float azimuthKi`, `float elevationKp`, `float elevationKi`, `uint8 resetIntegrator` |
| 5 | `imuReferenceStatus` | telemetry | `uint8 enable`, `uint8 frameMode`, target/current/error/output/motor floats, folded target floats, `uint8 reverseBranch` |

`frameMode` values:

- `0`: BNO055 `IMU` mode.
- `1`: BNO055 `NDOF` mode.

Command ranges:

- `targetAzimuthDeg`: normalized to `0..360`.
- `targetElevationDeg`: clamped to `0..180` because it is controlled against BNO055 `eulerZ` and Motor2's servo range.
- `enable`: `0` disables IMU reference control, `1` enables it.
- `frameMode`: `0` or `1`.

## Control Behavior

The controller runs once per IMU sample, currently every 100 ms.

- Current feedback:
  - `currentAzimuthDeg = normalize360(eulerX)`
  - `currentElevationDeg = eulerZ`
- Error:
  - `azimuthErrorDeg = wrap180(targetAzimuthDeg - currentAzimuthDeg)`
  - `elevationErrorDeg = targetElevationDeg - currentElevationDeg`
- Normal candidate:
  - `az = targetAzimuthDeg`
  - `el = targetElevationDeg`
- Flipped candidate:
  - `az = targetAzimuthDeg + 180`
  - `el = 180 - targetElevationDeg`
- Mode selection is done on base candidates before feedback correction.
- Firmware prefers the normal candidate when it is inside servo limits; otherwise it tries flipped.
- This keeps targets such as azimuth `0` on the normal branch instead of staying flipped from an earlier unreachable target.
- If neither base candidate is inside limits, firmware clamps both and selects the one closest to the last logical command, then marks the mode as saturated.
- Feedback correction is small and applied only after base selection:
  - `delta = clamp(Kp * error, -max_step, +max_step)`
- Final command is clamped to servo limits, rate-limited against the last logical command, converted to physical motor direction, then sent to the motors.
- `imuReferenceTuning.azimuthKp` and `elevationKp` are active correction gains.
- `azimuthKi` and `elevationKi` are accepted for wire compatibility but do not affect control.
- `resetIntegrator` clears legacy output fields; there is no integrator in this controller.
- A manual `motor` command updates the stored last command and disables IMU reference control.

Status fields:

- `targetAzimuthDeg` and `targetElevationDeg` report the effective IMU reference used by firmware.
- `currentAzimuthDeg` and `currentElevationDeg` report direct BNO055 feedback.
- `azimuthErrorDeg` and `elevationErrorDeg` report direct target-minus-feedback error.
- `azimuthPiOutputDeg` and `elevationPiOutputDeg` are legacy field names; they now report the applied feedback correction before rate limiting.
- `motor1AngleDeg` and `motor2AngleDeg` report the last physical servo commands.
- `motor1TargetAngleDeg` and `motor2TargetAngleDeg` report the physical target before rate limiting.
- `reverseBranch` is `1` when FLIPPED is selected; saturated mode reports the selected branch.

## Test Flow

1. Open the Test backend/frontend.
2. Send `imuReferenceTuning` with conservative values such as `1.0, 0.0, 1.0, 0.0, 1`.
3. Send `imuReferenceControl` with `enable=1` and `frameMode=1`.
4. Watch `bno055Telemetry` and `imuReferenceStatus`; `currentAzimuthDeg/currentElevationDeg` should match `eulerX/eulerZ`.
5. For target `0, 60` and telemetry `eulerX=14.5, eulerZ=85.5`, status should initially show approximately `azimuthErrorDeg=-14.5` and `elevationErrorDeg=-25.5`.
6. Sweep `targetAzimuthDeg` across `180` and verify NORMAL/FLIPPED selection.
7. Send a manual `motor` command before manual motor testing. This disables IMU reference control and updates the controller command estimate.
