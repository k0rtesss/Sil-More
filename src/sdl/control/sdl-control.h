#ifndef INCLUDED_SDL_CONTROL_H
#define INCLUDED_SDL_CONTROL_H

/* Optional local automation. All input and rendering stays on the SDL thread. */
void sdl_control_init(int argc, char** argv);
void sdl_control_shutdown(void);
bool sdl_control_poll(bool waiting_for_input);
bool sdl_control_handle_event(const SDL_Event* event);
int sdl_control_wait_timeout(int timeout_ms);
bool sdl_control_present_modal(SDL_Renderer* renderer);
bool sdl_control_wait_modal_event(SDL_Event* event, Uint64 accept_after_ns);

#endif
