#ifndef VELAPET_DEMO_INPUT_H
#define VELAPET_DEMO_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the demo step source.
 *
 * A worker thread feeds VelaPet with simulated steps - automatically and on
 * KEY2 presses - so the task/reward/level/skin-unlock loop can be exercised
 * on hardware that has no real step source.  The worker never touches LVGL;
 * an LVGL timer started here refreshes the UI, so this must be called from
 * the LVGL thread.
 *
 * Returns 0 on success, <0 when the worker could not be created.
 */
int velapet_demo_input_start(void);

#ifdef __cplusplus
}
#endif

#endif
