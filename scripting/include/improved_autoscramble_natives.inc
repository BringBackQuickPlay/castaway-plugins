// Natives. Must be included in improved_autoscramble.sp,
// and then in ANY plugin that wants to use the natives.

#if defined _improved_autoscramble_included
  #endinput
#endif
#define _improved_autoscramble_included

/**
 * Returns the Improved Autoscramble API version.
 *
 * This native exists ONLY so other plugins can safely detect
 * that the Improved Autoscramble library is present and compatible.
 *
 * @return API version number
 */
native int ImprovedAutoscramble_GetAPIVersion();

/**
 * Returns whether autoscramble is currently in progress.
 *
 * @return bool
 */
native bool Autoscramble_IsInProgress();

/**
 * Returns the next game time (GetGameTime) at which a scramble vote is allowed.
 * You can do something like Autoscramble_GetNextScrambleVoteAllowedTime() - GetGameTime() to
 * get the time left in seconds.
 *
 * @return float
 */
native float Autoscramble_GetNextScrambleVoteAllowedTime();