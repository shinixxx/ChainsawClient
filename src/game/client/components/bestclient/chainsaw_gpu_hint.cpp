/* ChainsawClient: ask the driver for the fast graphics card.
   On laptops with two graphics cards (built-in Intel/AMD + NVIDIA or AMD) the driver starts games on the slow built-in one
   unless the program says otherwise. These two exported values are the documented way to say it:
   NVIDIA Optimus reads NvOptimusEnablement, AMD switchable graphics reads AmdPowerXpressRequestHighPerformance.
   On a PC with one graphics card nothing changes. */
#if defined(CONF_FAMILY_WINDOWS) || defined(_WIN32)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
