int PollSDLEvents(zest_context context, SDL_Event *event) {
	int running = 1;
	while (SDL_PollEvent(event)) {
		ImGui_ImplSDL2_ProcessEvent(event);
		if (event->type == SDL_QUIT) {
			running = 0;
		}
		if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE && event->window.windowID == SDL_GetWindowID((SDL_Window*)zest_Window(context))) {
			running = 0;
		}
	}
	return running;
}
