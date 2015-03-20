### High ###

  * Implement desmume hotkey checking system without freezing gui
  * The screenshot display when loading a state is broken for gtk pure software, opengl doesn't display correctly due to glzoompixels or whatever.  It needs to save the vidsoft framebuffer for software mode I guess.
  * When resuming, any pressed keys will get stuck and record into the movie.
  * Gamepad input will randomly stop working, requiring you to enter the input menu to get it restarted
  * The ram search window doesn't display properly the second time you open it in a session
> > adelikat: Are you sure this is still valid?  It doesn't happen for me.
  * Compilation is broken on other platforms
  * Clean up the savestates / anything else now that the old movie system is gone
  * Resolve these random crash problems
  * Certain things will cut the fps in half, and it will remain that way for the rest of the session.  If you restart the emulator you'll be back up to normal fps.

### Medium ###

  * Ram Watch - off by one, 060143EA in ram search ends up being 060143EB in Ram Watch
### Low ###

  * Png dumper
  * Auto-hold