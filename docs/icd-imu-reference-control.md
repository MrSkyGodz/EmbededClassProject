# IMU Reference Control ICD

This feature controls a two-servo gimbal from an IMU-referenced azimuth/elevation target. The wire format is unchanged: the upper system still sends azimuth/elevation floats, and firmware converts IMU error into servo position corrections.

## Coordinate Mapping

- `targetAzimuthDeg` and `targetElevationDeg` are IMU references, not raw motor commands.
- Azimuth feedback is direct BNO055 telemetry `eulerX`.
- Elevation feedback is direct BNO055 telemetry `eulerZ`.
- Motor1 drives azimuth and remains physically inverted in firmware.
- Motor2 drives elevation directly.
- Both servos are treated as `0..180` position actuators.
- The controller does not use encoder feedback. It uses BNO055 feedback for actual look direction and the last sent motor command as the motor-position estimate.
- FLIPPED/reverse branch behavior is disabled. The controller uses only the normal branch and clamps at servo limits.

## ICD Types

All ICD payloads use little-endian floats and avoid C struct padding on the wire.

| ICD Type | Name | Direction | Payload |
| --- | --- | --- | --- |
| 3 | `imuReferenceControl` | command | `float targetAzimuthDeg`, `float targetElevationDeg`, `uint8 enable`, `uint8 frameMode` |
| 4 | `imuReferenceTuning` | command | `float azimuthKp`, `float azimuthKi`, `float elevationKp`, `float elevationKi`, `uint8 resetIntegrator` |
| 5 | `imuReferenceStatus` | telemetry | `uint8 enable`, `uint8 frameMode`, target/current/error/output/motor floats, `uint8 reverseBranch` |

`frameMode` values:

- `0`: BNO055 `IMU` mode.
- `1`: BNO055 `NDOF` mode.

Firmware starts BNO055 in `NDOF` mode by default, and controller status starts with `frameMode=1`. Use `frameMode=0` only when magnetometer/world heading should be ignored.

Command ranges:

- `targetAzimuthDeg`: normalized to `0..360`.
- `targetElevationDeg`: clamped to `0..180`.
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
- Feedback correction:
  - `activeError = error - sign(error) * deadband`, or `0` while inside deadband.
  - `motionScale` is reduced while BNO055 gyro magnitude is high, and becomes `0` during fast motion.
  - `delta = clamp(Kp * activeError * motionScale, -max_step, +max_step)`
- Because there is no encoder, correction is applied incrementally to the last logical command:
  - `cmd.az = last_cmd.az + (azimuthFeedbackDirection * delta_az)`
  - `cmd.el = last_cmd.el + (elevationFeedbackDirection * delta_el)`
- Final command is clamped to servo limits, rate-limited against the last logical command, converted to physical motor direction, then sent to the motors.
- No alternate branch is selected at limits. If a correction would move outside `0..180`, it is clamped.
- `imuReferenceTuning.azimuthKp` and `elevationKp` are active correction gains.
- `azimuthKi` and `elevationKi` are accepted for wire compatibility but do not affect control.
- `resetIntegrator` clears legacy output fields; there is no integrator in this controller.
- A manual `motor` command updates the stored last command and disables IMU reference control.

## IMU Recovery

- If BNO055 reads fail repeatedly, firmware closes the BNO055 I2C hardware path and re-runs `BNO055_Init()`.
- Recovery starts after 3 consecutive failed read cycles.
- If recovery fails, firmware waits 10 read cycles before trying again.
- After successful recovery, the last requested `frameMode` is applied again.
- No stale telemetry is published while reads are failing.

Status fields:

- `targetAzimuthDeg` and `targetElevationDeg` report the effective IMU reference used by firmware.
- `currentAzimuthDeg` and `currentElevationDeg` report direct BNO055 feedback.
- `azimuthErrorDeg` and `elevationErrorDeg` report target-minus-feedback error.
- `azimuthPiOutputDeg` and `elevationPiOutputDeg` are legacy field names; they now report the applied feedback correction before rate limiting.
- `motor1AngleDeg` and `motor2AngleDeg` report the last physical servo commands.
- `motor1TargetAngleDeg` and `motor2TargetAngleDeg` report the physical target before rate limiting.
- `reverseBranch` is always `0`.

## Test Flow

1. Open the Test backend/frontend.
2. Send `imuReferenceTuning` with conservative values such as `1.0, 0.0, 1.0, 0.0, 1`.
3. Send `imuReferenceControl` with `enable=1` and `frameMode=1`.
4. Watch `bno055Telemetry` and `imuReferenceStatus`; `currentAzimuthDeg/currentElevationDeg` should match `eulerX/eulerZ`.
5. Verify that `reverseBranch` remains `0` for all targets.
6. Drive targets near servo limits and verify motor commands clamp instead of selecting a flipped branch.
7. Send a manual `motor` command before manual motor testing. This disables IMU reference control and updates the controller command estimate.
