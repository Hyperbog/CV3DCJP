/* JP0623: game-owned pure menu state adapter called by pinned SDL host.
 * The platform executes returned actions via existing safe save/load/quit. */
#ifndef CVIII_START_HOST_0623_H
#define CVIII_START_HOST_0623_H
int cviii_pause_active(void);
int cviii_pause_toggle(void);
void cviii_pause_close(void);
void cviii_pause_move(int direction); /* -1 up, +1 down */
int cviii_pause_accept(void); /* CviiiStartAction enum, 0 if no action */
int cviii_pause_audio_enter(void); /* child preserves parent menu selection */
/* JP0625: current-slot Save/Load page, still using host-native state I/O. */
int cviii_pause_slot_active(void);
int cviii_pause_slot_begin(int action,unsigned current_slot);
void cviii_pause_slot_cancel(void);
int cviii_pause_slot_step(int direction);
int cviii_pause_slot_accept(unsigned *chosen_slot);
#endif
