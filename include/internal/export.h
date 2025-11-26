#ifdef LIBAEON_EXPORTS
  #define LIBAEON_API __declspec(dllexport)
#else
  #define LIBAEON_API __declspec(dllimport)
#endif