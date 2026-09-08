#ifndef INCLUDED_TUTORIAL_WORLD_H
#define INCLUDED_TUTORIAL_WORLD_H

/* Both calls belong to the completed player checkpoint, never the renderer.
 * Start snapshots current state without offering historical events on load. */
void tutorial_world_start(void);
void tutorial_world_checkpoint(void);

#endif
