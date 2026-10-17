#ifndef CONFIG_H
#define CONFIG_H

/**
 * @brief Enables debug features, mainly the boot debug menu and the pause debug menu, among other things
 * 
 * (see https://github.com/metroidret/mzm/blob/master/docs/debug.md for more details)
 * 
 */
// #define DEBUG

/**
 * @brief Enables non matching code (mostly identical behavior)
 * 
 */
// #define NON_MATCHING

/**
 * @brief Indicates that the game is being compiled using a modern compiler
 * This enables certain features that agbcc lacks
 * 
 */
// #define MODERN

/**
 * @brief Enables padding the ram with dummy values to get a matching rom
 * 
 */
#define RAM_PADDING

/**
 * @brief Enables various fixes to known bugs, issues, or bad code.
 * 
 * (see https://github.com/metroidret/mzm/blob/master/docs/bugs_and_glitches.md for reference)
 * 
 */
// #define BUGFIX

/**
 * @brief Enables the use of actual symbols instead of hardcoded pointers for things in Ewram
 * 
 */
// #define USE_EWRAM_SYMBOLS

/**
 * @brief Native build for PC/Android (mzm-android port). Implies MODERN-style
 * compilation, but keeps the hardcoded EWRAM offsets (they resolve against the
 * port's emulated EWRAM buffer, see include/port/platform.h).
 * Defined by the port build system, never for the GBA ROM.
 */
#ifdef PLATFORM_PC
#define MODERN
#define NON_MATCHING
#undef RAM_PADDING
#endif

// Automatically change a few flags if modern is enabled
#ifdef MODERN
// We always want bug fixes if we're using a modern compiler
#define BUGFIX
// Proper ewram symbols are also preferable
#ifndef PLATFORM_PC
#define USE_EWRAM_SYMBOLS
#endif
// Enable fake matching code, behavior should be identical
#define NON_MATCHING
// Ram padding is no longer necessary
#undef RAM_PADDING
#endif

#ifdef PORT_NO_BUGFIX
#undef BUGFIX
#endif

#endif /* CONFIG_H*/
