# Timer API

Timing and fixed timestep utilities.

## Creation

### zest_CreateTimer

Create timer with target update frequency.

```cpp
zest_timer zest_CreateTimer(double update_frequency_hz);
```

**Example:**

```cpp
zest_timer timer = zest_CreateTimer(60.0);  // 60 updates per second
```

---

## Configuration

### zest_TimerSetUpdateFrequency

```cpp
void zest_TimerSetUpdateFrequency(zest_timer *timer, double frequency_hz);
```

---

### zest_TimerSetMaxFrames

Set maximum accumulated frames (prevents spiral of death).

```cpp
void zest_TimerSetMaxFrames(zest_timer *timer, int max_frames);
```

---

### zest_TimerReset

```cpp
void zest_TimerReset(zest_timer *timer);
```

---

## Per-Frame

### zest_TimerTick

Update timer. Call once per frame.

```cpp
void zest_TimerTick(zest_timer *timer);
```

---

### zest_TimerDeltaTime

Get time since last tick (seconds).

```cpp
double zest_TimerDeltaTime(zest_timer *timer);
```

---

## Fixed Timestep Loop

### zest_TimerAccumulate / zest_TimerUnAccumulate

```cpp
void zest_TimerAccumulate(zest_timer *timer);
zest_bool zest_TimerUnAccumulate(zest_timer *timer);
```

### zest_TimerDoUpdate

Check if update should run.

```cpp
zest_bool zest_TimerDoUpdate(zest_timer *timer);
```

### zest_TimerPendingTicks

Get number of pending updates.

```cpp
int zest_TimerPendingTicks(zest_timer *timer);
```

---

## Helper Macros

```cpp
// Fixed timestep loop
zest_StartTimerLoop(timer) {
    // Game logic at fixed rate
    UpdatePhysics(zest_TimerFrameLength(&timer));
} zest_EndTimerLoop(timer);
```

---

## Interpolation

### zest_TimerLerp

Get interpolation factor for smooth rendering.

```cpp
float zest_TimerLerp(zest_timer *timer);
```

**Example:**

```cpp
// Update at fixed rate
zest_StartTimerLoop(timer) {
    previous_pos = current_pos;
    current_pos = UpdatePosition();
} zest_EndTimerLoop(timer);

// Render with interpolation
float t = zest_TimerLerp(&timer);
render_pos = zest_LerpVec3(previous_pos, current_pos, t);
```

---

### zest_TimerFrameLength

Get fixed frame length (1/frequency).

```cpp
double zest_TimerFrameLength(zest_timer *timer);
```

---

### zest_TimerUpdateTime

Get total elapsed time.

```cpp
double zest_TimerUpdateTime(zest_timer *timer);
```

---

## See Also

- [First Application](../getting-started/first-application.md) - Timer usage in main loop
