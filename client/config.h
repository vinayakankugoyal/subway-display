// Tuning constants, the same on every machine. Anything environment-specific
// (WiFi, the server address) lives in secrets.h, which is gitignored.
#ifndef CONFIG_H
#define CONFIG_H


#define POLL_INTERVAL_MS 30000     // how often to re-fetch
#define HTTP_TIMEOUT_MS  8000      // per-request timeout
#define FULL_REFRESH_MS  900000    // full refresh this often, to clear ghosting
#define STAMP_MAX_AGE_MS 300000    // redraw at least this often, to keep the
                                   // header timestamp honest when nothing moves

#define MAX_ROWS         4       // display can show this many lines
#define MAX_MINS         4       // countdown values kept per row

#define HEADER_H         34      // px reserved at the top for the timestamp

#endif /* CONFIG_H */
