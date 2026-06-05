# IMU Reference Control ICD

This feature controls a two-servo gimbal from an IMU-referenced azimuth/elevation target. The wire format is unchanged: the upper system sends azimuth/elevation floats, and firmware converts IMU error into servo position corrections.

## Coordinate Mapping

- `targetAzimuthDeg` and `targetElevationDeg` are IMU references, not raw motor commands.
- Azimuth feedback is direct BNO055 telemetry `eulerX`.
- Elevation feedback is direct BNO055 telemetry `eulerZ`.
- Motor1 drives azimuth and remains physically inverted in firmware.
- Motor2 drives elevation directly.
- Both servos are treated as `0..180` position actuators.
- The controller does not use encoder feedback. It uses BNO055 feedback for actual look direction and the last sent motor command as the motor-position estimate.
- `reverseBranch` is not used by the controller and is always reported as `0`.

## ICD Types

All ICD payloads use little-endian floats and avoid C struct padding on the wire.

| ICD Type | Name | Direction | Payload |
| --- | --- | --- | --- |
| 2 | `bno055Telemetry` | telemetry | `float eulerX`, `float eulerY`, `float eulerZ`, `float gyroX`, `float gyroY`, `float gyroZ` |
| 3 | `imuReferenceControl` | command | `float targetAzimuthDeg`, `float targetElevationDeg`, `uint8 enable`, `uint8 frameMode` |
| 4 | `imuReferenceTuning` | command | `float azimuthKp`, `float azimuthKi`, `float elevationKp`, `float elevationKi`, `uint8 resetIntegrator` |
| 5 | `imuReferenceStatus` | telemetry | `uint8 enable`, `uint8 frameMode`, target/current/error/output/motor floats, `uint8 reverseBranch` |
| 6 | `bno055CalibrationStatus` | telemetry | `uint8 system`, `uint8 gyro`, `uint8 acc`, `uint8 mag`, `uint8 fullyCalibrated` |

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
  - `delta = clamp(Kp * error, -max_step, +max_step)`
- Because there is no encoder, correction is applied incrementally to the last logical command:
  - `cmd.az = clamp(last_cmd.az + delta_az, 0, 180)`
  - `cmd.el = clamp(last_cmd.el + delta_el, 0, 180)`
- Final command is converted to physical motor direction, then sent to the motors.
- If a correction would move outside `0..180`, it is clamped.
- `imuReferenceTuning.azimuthKp` and `elevationKp` are active correction gains.
- `azimuthKi` and `elevationKi` are accepted for wire compatibility but are ignored.
- `resetIntegrator` clears legacy output fields; there is no integrator in this controller.
- A manual `motor` command updates the stored last command and disables IMU reference control.

## IMU Recovery

- If BNO055 reads fail repeatedly, firmware closes the BNO055 I2C hardware path and re-runs `BNO055_Init()`.
- Recovery starts after 3 consecutive failed read cycles.
- If recovery fails, firmware waits 10 read cycles before trying again.
- After successful recovery, the last requested `frameMode` is applied again.
- No stale telemetry is published while reads are failing.
- During normal sample publishing, firmware also reads BNO055 calibration status and publishes `bno055CalibrationStatus`.

Status fields:

- `targetAzimuthDeg` and `targetElevationDeg` report the effective IMU reference used by firmware.
- `currentAzimuthDeg` and `currentElevationDeg` report direct BNO055 feedback.
- `azimuthErrorDeg` and `elevationErrorDeg` report target-minus-feedback error.
- `azimuthPiOutputDeg` and `elevationPiOutputDeg` are legacy field names; they now report the applied feedback correction.
- `motor1AngleDeg` and `motor2AngleDeg` report the last physical servo commands.
- `motor1TargetAngleDeg` and `motor2TargetAngleDeg` match the last physical servo commands.
- `reverseBranch` is always `0`.

## Test Flow

1. Open the Test backend/frontend.
2. Send `imuReferenceTuning` with conservative values such as `1.0, 0.0, 1.0, 0.0, 1`.
3. Send `imuReferenceControl` with `enable=1` and `frameMode=1`.
4. Watch `bno055Telemetry` and `imuReferenceStatus`; `currentAzimuthDeg/currentElevationDeg` should match `eulerX/eulerZ`.
5. Verify that `reverseBranch` remains `0` for all targets.
6. Drive targets near servo limits and verify motor commands clamp.
7. Send a manual `motor` command before manual motor testing. This disables IMU reference control and updates the controller command estimate.

Hardware test cards:

- `imu_calibration_sweep`: runs the automatic pan-tilt calibration profile. It only verifies fields that this mechanism can reasonably calibrate by itself: `gyro=3` and `mag=3`. The mag phase moves pan and tilt together through a figure-eight-like path.
- `imu_manual_acc_system_calibration`: guides the operator through physical IMU orientations for `acc=3`, then asks for slow whole-unit rotations/tilts until `system=3` stays stable.
- `imu_axis_limit_rotation`: drives motor limits and checks that the selected Euler axis changes by the expected angle within `toleranceDeg`.
- `axis=2` is the default and runs azimuth then elevation automatically. `axis=0` checks only azimuth with `eulerX`; `axis=1` checks only elevation with `eulerZ`.
