#pragma once

/* Asset base folder — adjust to your layout */
#if defined(_XBOX) || defined(XBOX) || defined(_XBOX_)
#define DATA_DIR "D:\\assets"
#else
#define DATA_DIR "../assets"
#endif

/* Game version strings used by title/meta screens */
#ifndef VERSION
#define VERSION  "1.0"
#endif

#ifndef REVISION
#define REVISION "r1"
#endif
#pragma once
