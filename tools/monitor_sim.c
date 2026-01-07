/*
 * Monitor Simulator - Frame Timing Test Tool
 *
 * Tests frame timing algorithms against simulated monitor configurations.
 * Based on Tyler Glaiel's approach from "How to make your game run at 60fps"
 *
 * Build: gcc -O2 -o monitor_sim monitor_sim.c -lm
 * Usage: ./monitor_sim [algorithm] [monitor_hz] [num_frames]
 *
 * Algorithms:
 *   0 = Anchor current (naive accumulator)
 *   1 = Glaiel (vsync snapping + averaging)
 *   2 = Anchor fixed (proposed improvements)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

// Simulation clock (1MHz = microsecond precision)
#define SIM_FREQ 1000000
static int64_t sim_time = 0;
static int64_t next_vsync = 0;

// Configuration
static struct {
    double monitor_hz;          // Display refresh rate
    double target_update_hz;    // Game's target update rate
    double timer_noise_ms;      // OS timer jitter (gaussian-ish)
    double render_time_ms;      // Time spent rendering
    bool vsync_enabled;
    int num_frames;
    int algorithm;
} config = {
    .monitor_hz = 60.0,
    .target_update_hz = 60.0,
    .timer_noise_ms = 0.1,
    .render_time_ms = 1.0,
    .vsync_enabled = true,
    .num_frames = 10000,
    .algorithm = 0
};

// Statistics
static struct {
    int total_updates;
    int total_vsyncs;
    int double_updates;     // Frames with 2+ updates
    int triple_updates;     // Frames with 3+ updates
    int zero_updates;       // Frames with 0 updates
    double game_time;
    double system_time;
    int update_counts[64];  // Histogram of updates per frame
} stats = {0};

// Random number generation
static uint64_t rng_state = 12345;

static double rand_double(void) {
    // xorshift64
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (double)(rng_state & 0xFFFFFFFF) / (double)0xFFFFFFFF;
}

// Approximate gaussian noise using sum of uniforms
static double rand_noise(void) {
    double sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += rand_double() - 0.5;
    }
    return sum / 2.0;  // Roughly gaussian, range ~[-1, 1]
}

// Simulated SDL_GetPerformanceCounter
static int64_t sim_get_time(void) {
    return sim_time;
}

// Simulated SDL_GL_SwapWindow - blocks until vsync, adds render time
static void sim_swap_buffers(void) {
    // Add render time
    sim_time += (int64_t)(config.render_time_ms * SIM_FREQ / 1000.0);

    // VSync blocking
    if (config.vsync_enabled && sim_time < next_vsync) {
        sim_time = next_vsync;
    }

    // Add timer noise (simulates OS timer imprecision)
    sim_time += (int64_t)(config.timer_noise_ms * SIM_FREQ / 1000.0 * rand_noise());

    // Schedule next vsync
    // Use running counter to avoid drift in vsync timing itself
    next_vsync += (int64_t)(SIM_FREQ / config.monitor_hz);
}

// Convert ticks to seconds
static double ticks_to_sec(int64_t ticks) {
    return (double)ticks / (double)SIM_FREQ;
}

// Convert seconds to ticks
static int64_t sec_to_ticks(double sec) {
    return (int64_t)(sec * SIM_FREQ);
}

// ============================================================================
// Algorithm 0: Anchor Current (naive accumulator)
// ============================================================================

static void algo_anchor_current(void) {
    double physics_rate = 1.0 / config.target_update_hz;
    int max_updates = 10;

    int64_t last_time = sim_get_time();
    double physics_lag = 0.0;

    for (int frame = 0; frame < config.num_frames; frame++) {
        sim_swap_buffers();

        int64_t current_time = sim_get_time();
        double dt = ticks_to_sec(current_time - last_time);
        last_time = current_time;

        // Accumulate, cap to prevent spiral of death
        physics_lag += dt;
        if (physics_lag > physics_rate * max_updates) {
            physics_lag = physics_rate * max_updates;
        }

        // Fixed timestep updates
        int updates_this_frame = 0;
        while (physics_lag >= physics_rate) {
            // update()
            updates_this_frame++;
            stats.total_updates++;
            stats.game_time += physics_rate;
            physics_lag -= physics_rate;
        }

        // Track statistics
        stats.total_vsyncs++;
        stats.system_time = ticks_to_sec(current_time);

        if (updates_this_frame < 64) {
            stats.update_counts[updates_this_frame]++;
        }
        if (updates_this_frame == 0) stats.zero_updates++;
        if (updates_this_frame >= 2) stats.double_updates++;
        if (updates_this_frame >= 3) stats.triple_updates++;

        // Print pattern (first 200 frames only to avoid spam)
        if (frame < 200) {
            printf("%d", updates_this_frame > 9 ? 9 : updates_this_frame);
        }
    }
    printf(config.num_frames <= 200 ? "\n" : "...\n");
}

// ============================================================================
// Algorithm 1: Glaiel (vsync snapping + delta averaging)
// ============================================================================

static void algo_glaiel(void) {
    int64_t desired_frametime = sec_to_ticks(1.0 / config.target_update_hz);
    int64_t vsync_maxerror = (int64_t)(SIM_FREQ * 0.0002);  // 0.2ms tolerance

    // Snap frequencies for common refresh rates
    int64_t snap_frequencies[8];
    int64_t base_snap = sec_to_ticks(1.0 / config.monitor_hz);
    for (int i = 0; i < 8; i++) {
        snap_frequencies[i] = base_snap * (i + 1);
    }

    // Delta time averaging (ring buffer)
    const int time_history_count = 4;
    int64_t time_averager[4] = {desired_frametime, desired_frametime, desired_frametime, desired_frametime};
    int64_t averager_residual = 0;

    int64_t last_time = sim_get_time();
    int64_t frame_accumulator = 0;
    bool resync = true;

    for (int frame = 0; frame < config.num_frames; frame++) {
        sim_swap_buffers();

        int64_t current_time = sim_get_time();
        int64_t delta_time = current_time - last_time;
        last_time = current_time;

        // Handle timing anomalies
        if (delta_time > desired_frametime * 8) {
            delta_time = desired_frametime;
        }
        if (delta_time < 0) {
            delta_time = 0;
        }

        // VSync snapping
        for (int i = 0; i < 8; i++) {
            int64_t diff = delta_time - snap_frequencies[i];
            if (diff < 0) diff = -diff;
            if (diff < vsync_maxerror) {
                delta_time = snap_frequencies[i];
                break;
            }
        }

        // Delta time averaging
        for (int i = 0; i < time_history_count - 1; i++) {
            time_averager[i] = time_averager[i + 1];
        }
        time_averager[time_history_count - 1] = delta_time;

        int64_t averager_sum = 0;
        for (int i = 0; i < time_history_count; i++) {
            averager_sum += time_averager[i];
        }
        delta_time = averager_sum / time_history_count;
        averager_residual += averager_sum % time_history_count;
        delta_time += averager_residual / time_history_count;
        averager_residual %= time_history_count;

        // Add to accumulator
        frame_accumulator += delta_time;

        // Spiral of death protection
        if (frame_accumulator > desired_frametime * 8) {
            resync = true;
        }

        // Resync if needed
        if (resync) {
            frame_accumulator = 0;
            delta_time = desired_frametime;
            resync = false;
        }

        // Fixed timestep updates
        int updates_this_frame = 0;
        while (frame_accumulator >= desired_frametime) {
            // update()
            updates_this_frame++;
            stats.total_updates++;
            stats.game_time += 1.0 / config.target_update_hz;
            frame_accumulator -= desired_frametime;
        }

        // Track statistics
        stats.total_vsyncs++;
        stats.system_time = ticks_to_sec(current_time);

        if (updates_this_frame < 64) {
            stats.update_counts[updates_this_frame]++;
        }
        if (updates_this_frame == 0) stats.zero_updates++;
        if (updates_this_frame >= 2) stats.double_updates++;
        if (updates_this_frame >= 3) stats.triple_updates++;

        if (frame < 200) {
            printf("%d", updates_this_frame > 9 ? 9 : updates_this_frame);
        }
    }
    printf(config.num_frames <= 200 ? "\n" : "...\n");
}

// ============================================================================
// Algorithm 2: Anchor Fixed (proposed improvements)
// ============================================================================

static void algo_anchor_fixed(void) {
    int64_t desired_frametime = sec_to_ticks(1.0 / config.target_update_hz);
    int64_t vsync_maxerror = (int64_t)(SIM_FREQ * 0.0002);
    int max_updates = 10;

    // Snap frequencies
    int64_t snap_frequencies[8];
    int64_t base_snap = sec_to_ticks(1.0 / config.monitor_hz);
    for (int i = 0; i < 8; i++) {
        snap_frequencies[i] = base_snap * (i + 1);
    }

    int64_t last_time = sim_get_time();
    int64_t physics_lag = 0;  // Using int64 instead of double

    for (int frame = 0; frame < config.num_frames; frame++) {
        sim_swap_buffers();

        int64_t current_time = sim_get_time();
        int64_t delta_time = current_time - last_time;
        last_time = current_time;

        // Handle timing anomalies BEFORE adding to accumulator
        if (delta_time > desired_frametime * 8) {
            delta_time = desired_frametime;
        }
        if (delta_time < 0) {
            delta_time = 0;
        }

        // VSync snapping
        for (int i = 0; i < 8; i++) {
            int64_t diff = delta_time - snap_frequencies[i];
            if (diff < 0) diff = -diff;
            if (diff < vsync_maxerror) {
                delta_time = snap_frequencies[i];
                break;
            }
        }

        // Accumulate
        physics_lag += delta_time;

        // Cap accumulator (spiral of death protection)
        if (physics_lag > desired_frametime * max_updates) {
            physics_lag = desired_frametime * max_updates;
        }

        // Fixed timestep updates
        int updates_this_frame = 0;
        while (physics_lag >= desired_frametime) {
            // update()
            updates_this_frame++;
            stats.total_updates++;
            stats.game_time += 1.0 / config.target_update_hz;
            physics_lag -= desired_frametime;
        }

        // Track statistics
        stats.total_vsyncs++;
        stats.system_time = ticks_to_sec(current_time);

        if (updates_this_frame < 64) {
            stats.update_counts[updates_this_frame]++;
        }
        if (updates_this_frame == 0) stats.zero_updates++;
        if (updates_this_frame >= 2) stats.double_updates++;
        if (updates_this_frame >= 3) stats.triple_updates++;

        if (frame < 200) {
            printf("%d", updates_this_frame > 9 ? 9 : updates_this_frame);
        }
    }
    printf(config.num_frames <= 200 ? "\n" : "...\n");
}

// ============================================================================
// Algorithm 3: Anchor Final (120Hz physics, 60Hz render cap, vsync snapping)
// ============================================================================

static void algo_anchor_final(void) {
    // Configuration matching anchor.c
    double physics_rate = 1.0 / 120.0;
    double render_rate = 1.0 / 60.0;
    int64_t vsync_maxerror = (int64_t)(SIM_FREQ * 0.0002);
    int max_updates = 10;

    // Snap frequencies based on monitor refresh rate
    int64_t snap_frequencies[8];
    int64_t base_snap = sec_to_ticks(1.0 / config.monitor_hz);
    for (int i = 0; i < 8; i++) {
        snap_frequencies[i] = base_snap * (i + 1);
    }

    int64_t last_time = sim_get_time();
    double physics_lag = 0.0;
    double render_lag = 0.0;

    int render_count = 0;
    int updates_since_last_render = 0;

    for (int vsync = 0; vsync < config.num_frames; vsync++) {
        sim_swap_buffers();

        int64_t current_time = sim_get_time();
        double dt = ticks_to_sec(current_time - last_time);
        last_time = current_time;

        // Clamp delta time (handle anomalies)
        if (dt > physics_rate * max_updates) {
            dt = physics_rate;
        }
        if (dt < 0) {
            dt = 0;
        }

        // VSync snapping
        for (int i = 0; i < 8; i++) {
            int64_t dt_ticks = sec_to_ticks(dt);
            int64_t diff = dt_ticks - snap_frequencies[i];
            if (diff < 0) diff = -diff;
            if (diff < vsync_maxerror) {
                dt = ticks_to_sec(snap_frequencies[i]);
                break;
            }
        }

        // Accumulate physics lag
        physics_lag += dt;
        if (physics_lag > physics_rate * max_updates) {
            physics_lag = physics_rate * max_updates;
        }

        // Accumulate render lag (capped)
        render_lag += dt;
        if (render_lag > render_rate * 2) {
            render_lag = render_rate * 2;
        }

        // Fixed timestep physics updates
        int updates_this_vsync = 0;
        while (physics_lag >= physics_rate) {
            updates_this_vsync++;
            stats.total_updates++;
            stats.game_time += physics_rate;
            physics_lag -= physics_rate;
        }

        // Render at 60Hz cap
        int rendered_this_vsync = 0;
        if (render_lag >= render_rate) {
            render_lag -= render_rate;
            rendered_this_vsync = 1;
            render_count++;
        }

        // Track statistics (per rendered frame, not per vsync)
        stats.total_vsyncs++;
        stats.system_time = ticks_to_sec(current_time);

        // For pattern output, show updates per RENDERED frame
        // We print when we render, showing how many updates happened since last render
        updates_since_last_render += updates_this_vsync;

        if (rendered_this_vsync) {
            if (render_count <= 200) {
                printf("%d", updates_since_last_render > 9 ? 9 : updates_since_last_render);
            }

            if (updates_since_last_render < 64) {
                stats.update_counts[updates_since_last_render]++;
            }
            if (updates_since_last_render == 0) stats.zero_updates++;
            if (updates_since_last_render >= 2) stats.double_updates++;
            if (updates_since_last_render >= 3) stats.triple_updates++;

            updates_since_last_render = 0;
        }
    }
    printf(render_count <= 200 ? "\n" : "...\n");

    printf("  (Rendered %d frames from %d vsyncs)\n", render_count, config.num_frames);
}

// ============================================================================
// Main
// ============================================================================

static void print_usage(const char* prog) {
    printf("Monitor Simulator - Frame Timing Test Tool\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -a, --algorithm N    Timing algorithm (0=anchor, 1=glaiel, 2=fixed)\n");
    printf("  -m, --monitor HZ     Monitor refresh rate (default: 60)\n");
    printf("  -t, --target HZ      Target update rate (default: 60)\n");
    printf("  -n, --frames N       Number of frames to simulate (default: 10000)\n");
    printf("  -j, --jitter MS      Timer noise in ms (default: 0.1)\n");
    printf("  -r, --render MS      Render time in ms (default: 1.0)\n");
    printf("  -v, --vsync 0|1      VSync enabled (default: 1)\n");
    printf("  --all                Run all algorithms for comparison\n");
    printf("  -h, --help           Show this help\n\n");
    printf("Algorithms:\n");
    printf("  0 = Anchor old       Naive accumulator (old anchor.c)\n");
    printf("  1 = Glaiel           VSync snapping + delta averaging\n");
    printf("  2 = Anchor fixed     Snapping only (no render cap)\n");
    printf("  3 = Anchor final     120Hz physics, 60Hz render cap, snapping\n\n");
    printf("Examples:\n");
    printf("  %s -m 59.94                    # Test 59.94Hz monitor\n", prog);
    printf("  %s -m 144 -t 60                # 144Hz monitor, 60Hz updates\n", prog);
    printf("  %s --all -m 59.94 -n 100000    # Compare all on 59.94Hz\n", prog);
}

static void reset_stats(void) {
    memset(&stats, 0, sizeof(stats));
}

static void reset_simulation(void) {
    sim_time = 0;
    next_vsync = sec_to_ticks(1.0 / config.monitor_hz);
    rng_state = 12345;  // Deterministic
}

static void run_algorithm(int algo) {
    reset_stats();
    reset_simulation();

    const char* names[] = {"Anchor Old", "Glaiel", "Anchor Fixed", "Anchor Final"};
    printf("\n=== %s ===\n", names[algo]);
    printf("Config: %.2fHz monitor, %.2fHz target, vsync=%s, noise=%.2fms, render=%.2fms\n",
           config.monitor_hz, config.target_update_hz,
           config.vsync_enabled ? "on" : "off",
           config.timer_noise_ms, config.render_time_ms);
    printf("Pattern: ");

    switch (algo) {
        case 0: algo_anchor_current(); break;
        case 1: algo_glaiel(); break;
        case 2: algo_anchor_fixed(); break;
        case 3: algo_anchor_final(); break;
    }

    printf("\nResults:\n");
    printf("  Total updates:    %d\n", stats.total_updates);
    printf("  Total vsyncs:     %d\n", stats.total_vsyncs);
    printf("  Zero updates:     %d (%.2f%%)\n", stats.zero_updates,
           100.0 * stats.zero_updates / stats.total_vsyncs);
    printf("  Double updates:   %d (%.2f%%)\n", stats.double_updates,
           100.0 * stats.double_updates / stats.total_vsyncs);
    printf("  Triple+ updates:  %d (%.2f%%)\n", stats.triple_updates,
           100.0 * stats.triple_updates / stats.total_vsyncs);
    printf("  Game time:        %.3f sec\n", stats.game_time);
    printf("  System time:      %.3f sec\n", stats.system_time);
    printf("  Drift:            %+.3f ms\n", (stats.game_time - stats.system_time) * 1000.0);

    // Update distribution
    printf("  Distribution:     ");
    for (int i = 0; i < 8; i++) {
        if (stats.update_counts[i] > 0) {
            printf("[%d]=%d ", i, stats.update_counts[i]);
        }
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    bool run_all = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--algorithm") == 0) {
            config.algorithm = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--monitor") == 0) {
            config.monitor_hz = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0) {
            config.target_update_hz = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--frames") == 0) {
            config.num_frames = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jitter") == 0) {
            config.timer_noise_ms = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--render") == 0) {
            config.render_time_ms = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--vsync") == 0) {
            config.vsync_enabled = atoi(argv[++i]) != 0;
        }
        else if (strcmp(argv[i], "--all") == 0) {
            run_all = true;
        }
    }

    printf("Monitor Simulator - Frame Timing Test Tool\n");
    printf("============================================\n");

    if (run_all) {
        for (int algo = 0; algo < 4; algo++) {
            run_algorithm(algo);
        }
    } else {
        run_algorithm(config.algorithm);
    }

    return 0;
}
