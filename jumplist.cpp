#include <objectarray.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <string>
#include <sstream>
#include <map>
#include <strsafe.h>
#include "jumplist.h"
#include "api.h"

JumpList::JumpList(const std::wstring AppID, const bool delete_now) : pcdl(NULL)
{
	HRESULT hr = CoCreateInstance(CLSID_DestinationList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pcdl));
	if (SUCCEEDED(hr) && (pcdl != NULL))
	{
		pcdl->SetAppID(AppID.c_str());

		IApplicationDocumentLists *padl = NULL;
		hr = CoCreateInstance(CLSID_ApplicationDocumentLists, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&padl));
		if (SUCCEEDED(hr) && padl)
		{
			CleanJL(AppID, padl, ADLT_RECENT);
			CleanJL(AppID, padl, ADLT_FREQUENT);
			padl->Release();
		}

		if (delete_now)
		{
			pcdl->DeleteList(NULL);
		}
	}
}

JumpList::~JumpList()
{
	if (pcdl != NULL)
	{
		pcdl->Release();
	}
}

// Creates a CLSID_ShellLink to insert into the Tasks section of the Jump List. This type of
// Jump List item allows the specification of an explicit command line to execute the task.
HRESULT JumpList::_CreateShellLink(const std::wstring &path, PCWSTR pszArguments,
								   PCWSTR pszTitle, IShellLink **ppsl,
								   const int iconindex, const int mode)
{
	IShellLink *psl = NULL;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
	if (SUCCEEDED(hr))
	{
		if (psl)
		{
			psl->SetIconLocation(path.c_str(), iconindex);
			if (mode)
			{
				wchar_t fname[MAX_PATH] = {0};
				GetModuleFileName(0, fname, MAX_PATH);
				PathRenameExtension(fname, L".exe");

				if (mode == 1)
				{
					wchar_t shortfname[MAX_PATH] = {0};
					GetShortPathName(fname, shortfname, MAX_PATH);
					hr = psl->SetPath(shortfname);
				}
				else
				{
					hr = psl->SetPath(fname);
				}
			}
			else
			{
				hr = psl->SetPath(L"rundll32.exe");
			}

			if (SUCCEEDED(hr))
			{
				hr = psl->SetArguments(pszArguments);

				if (SUCCEEDED(hr))
				{
					// The title property is required on Jump List items provided as an IShellLink
					// instance.  This value is used as the display name in the Jump List.
					IPropertyStore *pps = NULL;
					hr = psl->QueryInterface(IID_PPV_ARGS(&pps));
					if (SUCCEEDED(hr))
					{
						PROPVARIANT propvar = {0};
						hr = InitPropVariantFromString(pszTitle, &propvar);
						if (SUCCEEDED(hr))
						{
							hr = pps->SetValue(PKEY_Title, propvar);
							if (SUCCEEDED(hr))
							{
								hr = pps->Commit();
								if (SUCCEEDED(hr))
								{
									hr = psl->QueryInterface(IID_PPV_ARGS(ppsl));
								}
							}
							PropVariantClear(&propvar);
						}
						pps->Release();
					}
				}
			}
			else
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
			}
			psl->Release();
		}
	}
	return hr;
}

void JumpList::CreateJumpList(const std::wstring &pluginpath, const std::wstring &pref,
							  const std::wstring &openfile, const std::wstring &bookmarks,
							  const std::wstring &pltext, const bool recent,
							  const bool frequent, const bool tasks, const bool addbm,
							  const bool playlist, const std::wstring &bms)
{
	UINT cMinSlots = 0;
	IObjectArray *poaRemoved = NULL;
	IObjectCollection *poc = NULL;
	HRESULT hr = pcdl->BeginList(&cMinSlots, IID_PPV_ARGS(&poaRemoved));

	CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&poc));

	if (!bms.empty() && addbm && hr == S_OK)
	{
		std::wstringstream ss(bms);
		std::wstring line1, line2;
		bool b = false;
		while (getline(ss, line1))
		{
			if (b)
			{
				IShellLink *psl = NULL;
				hr = _CreateShellLink(pluginpath, line2.c_str(), line1.c_str(), &psl, 2, 1);

				if (!_IsItemInArray(line2, poaRemoved))
				{
					psl->SetDescription(line2.c_str());
					poc->AddObject(psl);
				}

				psl->Release();
				b = false;
			}
			else
			{
				line2.resize(MAX_PATH);
				if (GetShortPathName(line1.c_str(), &line2[0], MAX_PATH) == 0)
				{
					line2 = line1;
				}
				else
				{
					line2.resize(wcslen(line2.c_str()));
				}

				b = true;
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		if (recent)
		{
			pcdl->AppendKnownCategory(KDC_RECENT);
		}

		if (frequent)
		{
			pcdl->AppendKnownCategory(KDC_FREQUENT);
		}
		
		if (addbm && !bms.empty())
		{
			_AddCategoryToList(poc, bookmarks);
		}

		if (playlist)
		{
			hr = _AddCategoryToList2(pluginpath, pltext);
		}

		if (tasks)
		{
			_AddTasksToList(pluginpath, pref, openfile);
		}

		if (SUCCEEDED(hr))
		{
			// Commit the list-building transaction.
			pcdl->CommitList();
		}
	}
	poaRemoved->Release();
	poc->Release();
}

HRESULT JumpList::_AddTasksToList(const std::wstring &pluginpath, const std::wstring &pref, const std::wstring &openfile)
{
	IObjectCollection *poc = NULL;
	HRESULT hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&poc));
	if (SUCCEEDED(hr))
	{
		if (poc)
		{
			IShellLink *psl = NULL;
			hr = _CreateShellLink(pluginpath, L"/COMMAND=40012", pref.c_str(), &psl, 0, 2);
			if (SUCCEEDED(hr))
			{
				hr = poc->AddObject(psl);
				psl->Release();
			}

			hr = _CreateShellLink(pluginpath, L"/COMMAND=40029", openfile.c_str(), &psl, 1, 2);
			if (SUCCEEDED(hr))
			{
				hr = poc->AddObject(psl);
				psl->Release();
			}

			IObjectArray *poa = NULL;
			hr = poc->QueryInterface(IID_PPV_ARGS(&poa));
			if (SUCCEEDED(hr))
			{
				if (poa)
				{
					// Add the tasks to the Jump List. Tasks always appear in the canonical "Tasks"
					// category that is displayed at the bottom of the Jump List, after all other
					// categories.
					hr = pcdl->AddUserTasks(poa);
					poa->Release();
				}
			}
			poc->Release();
		}
	}
	return hr;
}

// Determines if the provided IShellItem is listed in the array of items that the user has removed
bool JumpList::_IsItemInArray(std::wstring path, IObjectArray *poaRemoved)
{
	bool fRet = false;
	UINT cItems = 0;
	if (SUCCEEDED(poaRemoved->GetCount(&cItems)))
	{
		IShellLink *psiCompare = NULL;
		for (UINT i = 0; !fRet && i < cItems; i++)
		{
			if (SUCCEEDED(poaRemoved->GetAt(i, IID_PPV_ARGS(&psiCompare))))
			{
				if (psiCompare)
				{
					wchar_t removedpath[MAX_PATH] = {0};
					/*fRet = */(psiCompare->GetArguments(removedpath, MAX_PATH) == S_OK);
					fRet = !path.compare(removedpath);
					psiCompare->Release();
				}
			}
		}
	}
	return fRet;
}

// Adds a custom category to the Jump List.  Each item that should be in the category is added
// to an ordered collection, and then the category is appended to the Jump List as a whole.
HRESULT JumpList::_AddCategoryToList(IObjectCollection *poc, const std::wstring &bookmarks)
{
	IObjectArray *poa = NULL;
	HRESULT hr = poc->QueryInterface(IID_PPV_ARGS(&poa));
	if (SUCCEEDED(hr))
	{
		if (poa)
		{
			// Add the category to the Jump List.  If there were more categories,
			// they would appear from top to bottom in the order they were appended.
			hr = pcdl->AppendCategory(bookmarks.c_str(), poa);
			poa->Release();
		}
	}

	return hr;
}

// Adds a custom category to the Jump List.  Each item that should be in the category is added to
// an ordered collection, and then the category is appended to the Jump List as a whole.
HRESULT JumpList::_AddCategoryToList2(const std::wstring &pluginpath, const std::wstring &pltext)
{
	IObjectCollection *poc = NULL;
	HRESULT hr = CoCreateInstance(CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC, IID_PPV_ARGS(&poc));

	if (poc)
	{
		// enumerate through playlists (need to see if can use api_playlists.h via sdk)
		if (AGAVE_API_PLAYLISTS && AGAVE_API_PLAYLISTS->GetCount())
		{
			const size_t count = AGAVE_API_PLAYLISTS->GetCount();
			for (size_t i = 0; i < count; i++)
			{
				size_t numItems = 0;
				IShellLink *psl = NULL;

				wchar_t tmp[MAX_PATH] = {0};
				std::wstring title = AGAVE_API_PLAYLISTS->GetName(i);

				AGAVE_API_PLAYLISTS->GetInfo(i, api_playlists_itemCount, &numItems, sizeof(numItems));
				StringCchPrintf(tmp, MAX_PATH, L" [%d]", numItems);
				title += tmp;

				hr = _CreateShellLink(pluginpath, AGAVE_API_PLAYLISTS->GetFilename(i), title.c_str(), &psl, 3, 1);
				if (SUCCEEDED(hr))
				{
					if (psl)
					{
						psl->SetDescription(AGAVE_API_PLAYLISTS->GetFilename(i));
						hr = poc->AddObject(psl);
						psl->Release();
					}
				}
			}
		}

		IObjectArray *poa = NULL;
		hr = poc->QueryInterface(IID_PPV_ARGS(&poa));
		if (SUCCEEDED(hr))
		{
			if (poa)
			{
				// Add the category to the Jump List.  If there were more categories,
				// they would appear from top to bottom in the order they were appended.
				/*hr = */pcdl->AppendCategory(pltext.c_str(), poa);
				poa->Release();
			}
		}

		return S_OK;
	}

	return S_FALSE;
}

bool JumpList::CleanJL(const std::wstring AppID, IApplicationDocumentLists *padl, APPDOCLISTTYPE type)
{
	IObjectArray *poa = NULL;
	padl->SetAppID(AppID.c_str());
	HRESULT hr = padl->GetList(type, 0, IID_PPV_ARGS(&poa));

	if (SUCCEEDED(hr))
	{
		UINT *count = new UINT;
		hr = poa->GetCount(count);
		if (SUCCEEDED(hr) && (*count) > 100)
		{
			IApplicationDestinations *pad = NULL;
			hr = CoCreateInstance(CLSID_ApplicationDestinations, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pad));
			pad->SetAppID(AppID.c_str());

			if (SUCCEEDED(hr))
			{
				for (UINT i = (*count)-1; i > 100; --i)
				{
					IShellLink *psi = NULL;
					hr = poa->GetAt(i, IID_PPV_ARGS(&psi));

					if (SUCCEEDED(hr))
					{
						try
						{
							pad->RemoveDestination(psi);
						}
						catch (...)
						{
							continue;
						}
					}
				}
			}
		}
	}
	else
	{
		wchar_t path[MAX_PATH] = {0};
		SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path);
		std::wstring filepath(path);
		filepath += L"\\Microsoft\\Windows\\Recent\\AutomaticDestinations\\879d567ffa1f5b9f.automaticDestinations-ms";
		if (DeleteFile(filepath.c_str()) == 0)
		{
			return false;
		}
	}

	return true;
}