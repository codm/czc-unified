#ifndef CRON_H_
#define CRON_H_

#include "esp_err.h"
#include <cstdint>

/* Wildcard for any cron_timing_t field - matches every value */
constexpr uint8_t CRON_ANY = 0xFF;

/**
 * Struct to store cron job timing information
 *
 * @note `CRON_ANY` (`0xFF`) == wildcard
 */
typedef struct cron_timing_t {
    uint8_t minute = CRON_ANY;
    uint8_t hour = CRON_ANY;
    uint8_t dotm = CRON_ANY;
    uint8_t month = CRON_ANY;
    uint8_t weekday = CRON_ANY;
}; 

namespace Cron {
    /**
     * Creates and sets up Cron Task -> Task that runs every 60s and checks if any jobs need to be run 
     */
    esp_err_t init();

    /**
     * Schedules Job for Cron Task. Returns a Job handle (number) for later Job deletion.
     * 
     * @param[in] cron_timing When will the cron job run
     * @param[in] funcptr Function which is run 
     * @param[in] ctx pointer to function context
     * 
     * @note Cron functions must not have any parameters and need `esp_err_t` as a return value 
     * 
     * @returns Job handle - Number which can be used to later delete the Cron job. 
     * 
     *          `-1` on error.
     */
    int scheduleJob(cron_timing_t cron_timing, esp_err_t (*funcptr)(void* ctx), void* ctx);

    /** 
     * Removes job from scheduling list
     * 
     * @param[in] job_handle received on scheduling
     * 
     * @returns `ESP_OK` on success - `ESP_ERR_INVALID_ARGS` if handle is not found
     */
    esp_err_t removeJob(int job_handle);

    void close();
} /* cron */

#endif /* CRON_H_ */
