#pragma once

/** Which build this is, for the window to show: written by cmake/BuildInfo.cmake on every build. */
namespace mdt
{
/** Short commit SHA, "-dirty" when tracked files had uncommitted changes, "unknown" without git. */
const char* buildSha();
/** Local build time, "YYYY-MM-DD HH:MM". */
const char* buildDate();
} // namespace mdt
