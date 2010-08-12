// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DXFEED_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DXFEED_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DXFEED_EXPORTS
#define DXFEED_API __declspec(dllexport)
#else
#define DXFEED_API __declspec(dllimport)
#endif

// This class is exported from the DXFeed.dll
class DXFEED_API CDXFeed {
public:
	CDXFeed(void);
	// TODO: add your methods here.
};

extern DXFEED_API int nDXFeed;

DXFEED_API int fnDXFeed(void);
