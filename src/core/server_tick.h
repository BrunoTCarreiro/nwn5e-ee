#pragma once

// Every frame of the server's main loop (CServerExoApp::MainLoop), after it - paused or not. A paused game still
// handles the players' input but runs no DelayCommand and no creature update; this is the way to do something a frame
// or a few milliseconds later regardless (#171, 0.99.87).
using ServerTickListener = void (*)();
bool AddServerTickListener(ServerTickListener listener);
