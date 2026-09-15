#define EMPTY()
#define DEFER1(m) m EMPTY()

#define EVAL(...)     EVAL1024(__VA_ARGS__)
#define EVAL1024(...) EVAL512(EVAL512(__VA_ARGS__))
#define EVAL512(...)  EVAL256(EVAL256(__VA_ARGS__))
#define EVAL256(...)  EVAL128(EVAL128(__VA_ARGS__))
#define EVAL128(...)  EVAL64(EVAL64(__VA_ARGS__))
#define EVAL64(...)   EVAL32(EVAL32(__VA_ARGS__))
#define EVAL32(...)   EVAL16(EVAL16(__VA_ARGS__))
#define EVAL16(...)   EVAL8(EVAL8(__VA_ARGS__))
#define EVAL8(...)    EVAL4(EVAL4(__VA_ARGS__))
#define EVAL4(...)    EVAL2(EVAL2(__VA_ARGS__))
#define EVAL2(...)    EVAL1(EVAL1(__VA_ARGS__))
#define EVAL1(...)    __VA_ARGS__

#if defined(NDEBUG) || defined(_NDEBUG)
#undef MVC_DEBUG
#else
#define MVC_DEBUG
#endif

#if defined(_WIN32) || defined(_WIN64)
#define MVC_WIN(...) __VA_ARGS__
#define MVC_MAC(...)
#define MVC_LINUX(...)
#define MVC_UNIX(...)
#elif defined(__APPLE__)
#define MVC_WIN(...) 
#define MVC_MAC(...) __VA_ARGS__
#define MVC_LINUX(...)
#define MVC_UNIX(...) __VA_ARGS__
#elif defined(__linux__)
#define MVC_WIN(...) 
#define MVC_MAC(...)
#define MVC_LINUX(...) __VA_ARGS__
#define MVC_UNIX(...) __VA_ARGS__
#else
#error "what"
#endif


