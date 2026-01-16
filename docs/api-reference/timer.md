# Timer API

Timing and fixed timestep utilities for game loops and simulations. The timer system provides a fixed timestep loop with accumulator-based timing, which is essential for deterministic physics and smooth rendering with interpolation.

You can also just roll your own but this can cover a lot of needs.

## Timer Type

```cpp
typedef struct zest_timer_t {
    double start_time;
    double delta_time;
    double update_frequency;
    double update_tick_length;
    double update_time;
    double ticker;
    double accumulator;
    double accumulator_delta;
    double current_time;
    double lerp;
    double time_passed;
    double seconds_passed;
    double max_elapsed_time;
    int update_count;
} zest_timer_t;
```

---

## Creation

### `zest_CreateTimer`

Create a timer with a target update frequency.

```cpp
zest_timer_t zest_CreateTimer(double update_frequency);
```

- `update_frequency` - Target updates per second (Hz)

**Usage:** Initialize a timer for your game loop. Common frequencies are 60 Hz for gameplay or 120+ Hz for physics.

```cpp
zest_timer_t timer = zest_CreateTimer(60.0);   // 60 updates per second
zest_timer_t physics_timer = zest_CreateTimer(120.0);  // 120 Hz physics
```

---

## Configuration

### `zest_TimerSetUpdateFrequency`

Change the update frequency of an existing timer.

```cpp
void zest_TimerSetUpdateFrequency(zest_timer_t *timer, double update_frequency);
```

**Usage:** Dynamically adjust tick rate, such as for slow-motion effects or performance scaling.

```cpp
// Slow motion: halve the update rate
zest_TimerSetUpdateFrequency(&timer, 30.0);

// Return to normal
zest_TimerSetUpdateFrequency(&timer, 60.0);
```

---

### `zest_TimerSetMaxFrames`

Set the maximum accumulated time (prevents spiral of death).

```cpp
void zest_TimerSetMaxFrames(zest_timer_t *timer, double frames);
```

- `frames` - Maximum number of frames worth of time to accumulate

**Usage:** Prevent the simulation from running too many updates after a lag spike, which could cause further lag and create a "spiral of death."

```cpp
// Limit to 4 frames of accumulated time
zest_TimerSetMaxFrames(&timer, 4.0);
```

---

### `zest_TimerReset`

Reset the timer's clock to the current time.

```cpp
void zest_TimerReset(zest_timer_t *timer);
```

**Usage:** Reset timing after pausing, loading, or other interruptions to prevent large delta times.

```cpp
// After loading screen or pause menu
zest_TimerReset(&timer);
```

---

## Per-Frame Updates

### `zest_TimerTick`

Update the timer's delta time. Call once per frame before using delta time.

```cpp
void zest_TimerTick(zest_timer_t *timer);
```

**Usage:** Update the timer at the start of each frame to calculate elapsed time.

```cpp
while (running) {
    zest_TimerTick(&timer);

    // Use timer for updates and rendering
    // ...
}
```

---

### `zest_TimerDeltaTime`

Get the time elapsed since the last tick (in seconds).

```cpp
double zest_TimerDeltaTime(zest_timer_t *timer);
```

**Usage:** Get raw frame time for variable timestep operations like camera movement or UI animations.

```cpp
zest_TimerTick(&timer);
double dt = zest_TimerDeltaTime(&timer);

// Variable timestep camera movement
camera_pos.x += input_x * camera_speed * dt;
camera_pos.y += input_y * camera_speed * dt;
```

---

## Fixed Timestep Loop

Fixed timestep loops run game logic at a consistent rate regardless of frame rate, which is essential for deterministic physics and network synchronization.

### `zest_TimerAccumulate`

Add the elapsed time since last frame to the accumulator.

```cpp
double zest_TimerAccumulate(zest_timer_t *timer);
```

**Returns:** The accumulated time in seconds.

**Usage:** Called at the start of your fixed timestep loop to accumulate frame time.

```cpp
zest_TimerAccumulate(&timer);
while (zest_TimerDoUpdate(&timer)) {
    // Fixed rate update
    zest_TimerUnAccumulate(&timer);
}
```

---

### `zest_TimerDoUpdate`

Check if another fixed timestep update should run.

```cpp
zest_bool zest_TimerDoUpdate(zest_timer_t *timer);
```

**Returns:** `ZEST_TRUE` if the accumulator has enough time for another update.

**Usage:** Loop condition for fixed timestep updates.

```cpp
while (zest_TimerDoUpdate(&timer)) {
    UpdatePhysics(zest_TimerUpdateTime(&timer));
    zest_TimerUnAccumulate(&timer);
}
```

---

### `zest_TimerUnAccumulate`

Subtract one tick's worth of time from the accumulator.

```cpp
void zest_TimerUnAccumulate(zest_timer_t *timer);
```

**Usage:** Called after each fixed update to consume accumulated time.

```cpp
while (zest_TimerDoUpdate(&timer)) {
    // Run one fixed update
    UpdateGameLogic();
    zest_TimerUnAccumulate(&timer);  // Consume the time
}
```

---

### `zest_TimerPendingTicks`

Get the number of fixed updates that will run this frame.

```cpp
int zest_TimerPendingTicks(zest_timer_t *timer);
```

**Usage:** Useful for debugging or adjusting behavior based on update count.

```cpp
zest_TimerAccumulate(&timer);
int ticks = zest_TimerPendingTicks(&timer);
if (ticks > 3) {
    // Many updates pending - maybe skip some non-essential work
}
```

---

### `zest_TimerSet`

Update the lerp value after processing all fixed updates.

```cpp
void zest_TimerSet(zest_timer_t *timer);
```

**Usage:** Called after the fixed update loop to prepare the interpolation value for rendering.

```cpp
zest_TimerAccumulate(&timer);
while (zest_TimerDoUpdate(&timer)) {
    UpdateLogic();
    zest_TimerUnAccumulate(&timer);
}
zest_TimerSet(&timer);  // Prepare lerp for rendering
```

---

## Helper Macros

Convenience macros that wrap the fixed timestep loop pattern.

```cpp
#define zest_StartTimerLoop(timer)  zest_TimerAccumulate(&timer); \
    int pending_ticks = zest_TimerPendingTicks(&timer); \
    while (zest_TimerDoUpdate(&timer)) {

#define zest_EndTimerLoop(timer)    zest_TimerUnAccumulate(&timer); \
    } \
    zest_TimerSet(&timer);
```

**Usage:** Simplify the fixed timestep loop boilerplate.

```cpp
// Fixed timestep game loop
zest_StartTimerLoop(timer) {
    // This runs at fixed rate (e.g., 60 times per second)
    UpdatePhysics(zest_TimerUpdateTime(&timer));
    UpdateGameLogic();
} zest_EndTimerLoop(timer);

// Render with interpolation
float t = zest_TimerLerp(&timer);
RenderScene(t);
```

---

## Timing Information

### `zest_TimerFrameLength`

Get the fixed frame length in seconds (1.0 / frequency).

```cpp
double zest_TimerFrameLength(zest_timer_t *timer);
```

**Usage:** Get the fixed timestep duration for physics and game logic.

```cpp
double dt = zest_TimerFrameLength(&timer);  // e.g., 0.01666... for 60 Hz
velocity.y -= gravity * dt;
position = zest_AddVec3(position, zest_ScaleVec3(velocity, dt));
```

---

### `zest_TimerUpdateTime`

Get the update time (same as frame length, 1.0 / frequency).

```cpp
double zest_TimerUpdateTime(zest_timer_t *timer);
```

**Usage:** Alternative name for getting the fixed timestep duration.

```cpp
double fixed_dt = zest_TimerUpdateTime(&timer);
```

---

### `zest_TimerUpdateFrequency`

Get the current update frequency in Hz.

```cpp
double zest_TimerUpdateFrequency(zest_timer_t *timer);
```

**Usage:** Query the timer's configured update rate.

```cpp
double freq = zest_TimerUpdateFrequency(&timer);  // e.g., 60.0
```

---

### `zest_TimerUpdateWasRun`

Check if at least one fixed update was run this frame.

```cpp
zest_bool zest_TimerUpdateWasRun(zest_timer_t *timer);
```

**Usage:** Determine if game state changed this frame (useful for networking or replay systems).

```cpp
if (zest_TimerUpdateWasRun(&timer)) {
    // Game state was updated - send network snapshot
    SendNetworkUpdate();
}
```

---

## Interpolation

### `zest_TimerLerp`

Get the interpolation factor for smooth rendering between fixed updates.

```cpp
double zest_TimerLerp(zest_timer_t *timer);
```

**Returns:** A value between 0.0 and 1.0 representing progress toward the next fixed update.

**Usage:** Interpolate between previous and current state for smooth rendering at any frame rate.

```cpp
// Fixed update loop - store previous state
zest_StartTimerLoop(timer) {
    previous_position = current_position;
    current_position = UpdatePhysics();
} zest_EndTimerLoop(timer);

// Render with interpolation for smooth visuals
double t = zest_TimerLerp(&timer);
zest_vec3 render_pos = zest_LerpVec3(&previous_position, &current_position, (float)t);
DrawObject(render_pos);
```

---

## Complete Example

```cpp
// Initialize timer at 60 Hz
zest_timer_t timer = zest_CreateTimer(60.0);
zest_TimerSetMaxFrames(&timer, 4.0);  // Prevent spiral of death

// Game state (double-buffered for interpolation)
zest_vec3 prev_player_pos, player_pos;
zest_vec3 player_velocity = zest_Vec3Set(0, 0, 0);

while (running) {
    // Update timer
    zest_TimerTick(&timer);

    // Handle input (variable rate)
    ProcessInput();

    // Fixed timestep game loop
    zest_StartTimerLoop(timer) {
        double dt = zest_TimerFrameLength(&timer);

        // Save previous state for interpolation
        prev_player_pos = player_pos;

        // Physics update at fixed rate
        player_velocity.y -= 9.8f * dt;  // Gravity
        player_pos = zest_AddVec3(player_pos, zest_ScaleVec3(player_velocity, dt));

        // Collision detection
        HandleCollisions();

    } zest_EndTimerLoop(timer);

    // Render with interpolation
    double t = zest_TimerLerp(&timer);
    zest_vec3 render_pos = zest_LerpVec3(&prev_player_pos, &player_pos, (float)t);

    RenderScene(render_pos);
    PresentFrame();
}
```

---

## Why Fixed Timestep?

Fixed timestep loops provide several benefits:

1. **Deterministic Physics** - Same inputs always produce same results
2. **Stable Simulations** - No explosions from large delta times
3. **Network Sync** - Easier to synchronize game state across clients
4. **Replay Systems** - Record inputs, replay exactly
5. **Smooth Rendering** - Interpolation provides smooth visuals at any frame rate

The tradeoff is slightly more complex code, but the `zest_StartTimerLoop`/`zest_EndTimerLoop` macros minimize this.

---

## See Also

- [First Application](../getting-started/first-application.md) - Timer usage in main loop
- [Math API](math.md) - Interpolation functions for smooth rendering
