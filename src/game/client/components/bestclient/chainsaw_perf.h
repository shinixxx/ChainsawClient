/* ChainsawClient: "maximum optimization" mode (bc_perf_mode).
   Turning it on switches off everything heavy that is chosen with the bc_perf_* groups and writes what it changed to
   chainsaw/perf_restore.txt (setting, old value, new value). Turning it off puts the old values back, but only for the
   settings that still have the value the mode gave them: what you changed yourself meanwhile stays as you left it.
   The file keeps the state over a restart of the game. Update() is called every frame (cheap: compares two ints). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_PERF_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_PERF_H

class IConsole;
class IStorage;

namespace CcPerf
{
// applies or restores when bc_perf_mode or one of the bc_perf_* groups changed
void Update(IConsole *pConsole, IStorage *pStorage);
// how many settings the mode has changed right now (0 when it is off)
int NumChanged();
} // namespace CcPerf

#endif
