#include "status.h"
#include "stm32f4xx_hal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// ---------------------------------------------
// State for simulated GPS + environment sensors
// ---------------------------------------------

typedef struct
{
    // GPS
    float    lat;
    float    lon;
    uint8_t  fix;
    uint8_t  sats;

    // Env
    float    t_c;
    float    p_hpa;
    float    rh_pct;
    float    lux;

    // Last update times (ms)
    uint32_t gps_last_update_ms;
    uint32_t env_last_update_ms;
} status_state_t;

static status_state_t st;

// ---------------------------------------------
// Time base from build date/time
// ---------------------------------------------

static uint32_t time_base_ms      = 0;
static uint32_t base_sec_of_day   = 0;
static char     date_iso[11]; // "YYYY-MM-DD"

// Convert "Mmm" -> month number
static uint8_t month_from_str(const char *m)
{
    if (strncmp(m, "Jan", 3) == 0) return 1;
    if (strncmp(m, "Feb", 3) == 0) return 2;
    if (strncmp(m, "Mar", 3) == 0) return 3;
    if (strncmp(m, "Apr", 3) == 0) return 4;
    if (strncmp(m, "May", 3) == 0) return 5;
    if (strncmp(m, "Jun", 3) == 0) return 6;
    if (strncmp(m, "Jul", 3) == 0) return 7;
    if (strncmp(m, "Aug", 3) == 0) return 8;
    if (strncmp(m, "Sep", 3) == 0) return 9;
    if (strncmp(m, "Oct", 3) == 0) return 10;
    if (strncmp(m, "Nov", 3) == 0) return 11;
    if (strncmp(m, "Dec", 3) == 0) return 12;
    return 1;
}

// Parse __DATE__ ("Mmm dd yyyy") and __TIME__ ("hh:mm:ss")
// into ISO date string + seconds-of-day
static void init_build_date_time(void)
{
    const char *date = __DATE__; // e.g. "Dec 06 2025"
    const char *time = __TIME__; // e.g. "21:37:10"

    char m_str[4];
    char d_str[3];
    char y_str[5];

    // "Mmm dd yyyy"
    memcpy(m_str, date, 3);
    m_str[3] = '\0';

    memcpy(d_str, date + 4, 2);
    d_str[2] = '\0';

    memcpy(y_str, date + 7, 4);
    y_str[4] = '\0';

    uint8_t month = month_from_str(m_str);
    int day       = atoi(d_str);
    int year      = atoi(y_str);

    // Build "YYYY-MM-DD"
    snprintf(date_iso, sizeof(date_iso), "%04d-%02u-%02d", year, month, day);

    // Parse "hh:mm:ss"
    int hh = 0, mm = 0, ss = 0;
    (void)sscanf(time, "%d:%d:%d", &hh, &mm, &ss);

    base_sec_of_day = (uint32_t)(hh * 3600 + mm * 60 + ss);
    time_base_ms    = HAL_GetTick();
}

// Build ISO8601 time from build date + uptime
static void format_time_utc(char *buf, size_t len)
{
    uint32_t now_ms   = HAL_GetTick();
    uint32_t delta_s  = (now_ms - time_base_ms) / 1000U;
    uint32_t sec_day  = base_sec_of_day + delta_s;
    sec_day          %= (24U * 3600U);

    uint32_t h = sec_day / 3600U;
    uint32_t m = (sec_day / 60U) % 60U;
    uint32_t s = sec_day % 60U;

    // date_iso = "YYYY-MM-DD"
    snprintf(buf, len, "%sT%02lu:%02lu:%02luZ",
             date_iso,
             (unsigned long)h,
             (unsigned long)m,
             (unsigned long)s);
}

// ---------------------------------------------
// Pseudo-random jitter for sensors
// ---------------------------------------------

// Tiny helper for pseudo-random float in [-delta, +delta]
static float jitter(float delta)
{
    int r = rand() % 20001 - 10000; // [-10000; 10000]
    return (r / 10000.0f) * delta;
}

// ---------------------------------------------
// Public API
// ---------------------------------------------

void status_init(void)
{
    memset(&st, 0, sizeof(st));

    // Seed PRNG (fixed seed is fine for lab)
    srand(1234);

    // Initial GPS around Lviv
    st.lat  = 49.8397f;
    st.lon  = 24.0297f;
    st.fix  = 3;
    st.sats = 10;

    // Initial env conditions
    st.t_c    = 24.3f;
    st.p_hpa  = 1007.8f;
    st.rh_pct = 47.2f;
    st.lux    = 356.0f;

    uint32_t now = HAL_GetTick();
    st.gps_last_update_ms = now;
    st.env_last_update_ms = now;

    // Init time base from build date/time
    init_build_date_time();
}

// Call this ~1 time per second from main loop
void status_update(void)
{
    uint32_t now = HAL_GetTick();

    // Simulate occasional "stale" GPS:
    // 80% chance to update, 20% chance to skip (stale grows)
    if ((rand() % 100) < 80)
    {
        st.lat += jitter(0.0001f);   // ~11m
        st.lon += jitter(0.0001f);
        st.sats = 8 + (rand() % 4);  // 8..11 satellites
        st.gps_last_update_ms = now;
    }

    // Simulate occasional "stale" env:
    if ((rand() % 100) < 85)
    {
        st.t_c    += jitter(0.2f);
        st.p_hpa  += jitter(0.5f);
        st.rh_pct += jitter(1.0f);
        st.lux    += jitter(10.0f);

        // Clamp to sane ranges
        if (st.t_c   < -20.0f) st.t_c   = -20.0f;
        if (st.t_c   >  50.0f) st.t_c   =  50.0f;
        if (st.rh_pct < 0.0f)  st.rh_pct =  0.0f;
        if (st.rh_pct > 100.0f)st.rh_pct = 100.0f;
        if (st.lux   < 0.0f)   st.lux   =  0.0f;

        st.env_last_update_ms = now;
    }
}

void build_status_json(char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0)
        return;

    uint32_t now = HAL_GetTick();
    float gps_stale_s = (now - st.gps_last_update_ms) / 1000.0f;
    float env_stale_s = (now - st.env_last_update_ms) / 1000.0f;

    char time_buf[32];
    format_time_utc(time_buf, sizeof(time_buf));

    int n = snprintf(
        buf,
        buf_len,
        "{"
          "\"proto_ver\":1,"
          "\"device_id\":\"STM32-411-xxxx\","
          "\"time_utc\":\"%s\","
          "\"gps\":{"
              "\"lat\":%.6f,"
              "\"lon\":%.6f,"
              "\"fix\":%u,"
              "\"sats\":%u"
          "},"
          "\"env\":{"
              "\"t_c\":%.1f,"
              "\"p_hpa\":%.1f,"
              "\"rh_pct\":%.1f,"
              "\"lux\":%.1f"
          "},"
          "\"stale_age_s\":{"
              "\"gps\":%.1f,"
              "\"env\":%.1f"
          "}"
        "}",
        time_buf,
        st.lat, st.lon,
        (unsigned)st.fix, (unsigned)st.sats,
        st.t_c, st.p_hpa, st.rh_pct, st.lux,
        gps_stale_s, env_stale_s
    );

    if (n < 0 || (size_t)n >= buf_len)
    {
        buf[buf_len - 1] = '\0';
    }
}
