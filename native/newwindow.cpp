#include <Shellapi.h>

class browser
{
	public:

	
	static void launchBrowser(String address, String windowName)
	{
		LPCSTR addressStr = address.ToCString<char>();
		ShellExecute(HWND_DESKTOP, "open", addressStr, NULL, NULL, SW_SHOWNORMAL);
	}

};