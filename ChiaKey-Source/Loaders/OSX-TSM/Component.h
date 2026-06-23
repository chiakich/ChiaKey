// [AUTO_HEADER]

// AppleComponent stands for "Apple component return type"
#define AppleComponent pascal ComponentResult  // no extern "C" here

// main entry point
AppleComponent TSMCDispatch(ComponentParameters *param, Handle hndl);

// we implement these in this module
AppleComponent TSMCCanDo(SInt16 selector);
AppleComponent TSMCGetVersion();
AppleComponent TSMCGetScriptLangSupport(Handle hndl,
                                        ScriptLanguageSupportHandle *outhndl);

// component entry points
AppleComponent TSMCOpenComponent(ComponentInstance inst);
AppleComponent TSMCCloseComponent(Handle hndl, ComponentInstance inst);

// text service component entry points
AppleComponent TSMCInitiateTextService(Handle hndl);
AppleComponent TSMCTerminateTextService(Handle hndl);
AppleComponent TSMCActivateTextService(Handle hndl);
AppleComponent TSMCDeactivateTextService(Handle hndl);
AppleComponent TSMCTextServiceEvent(Handle hndl, EventRef evnt);
AppleComponent TSMCGetTextServiceMenu(Handle hndl, MenuHandle *mnu);
AppleComponent TSMCFixTextService(Handle hndl);
AppleComponent TSMCHidePaletteWindows(Handle hndl);

// initializer (pencil menu, bundle loading, call TSB init)
AppleComponent TSMCInitialize(ComponentInstance inst);

// menu handler
pascal OSStatus TSMCMenuHandler(EventHandlerCallRef callref, EventRef evnt,
                                void *userdata);
