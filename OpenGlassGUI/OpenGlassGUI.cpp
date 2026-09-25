#include "pch.h"
#include "OpenGlassGUI.hpp"
#include "MainFrame.hpp"
#include "Elevation.hpp"
#include "ConfigurationMigration.hpp"
#include <wx/cmdline.h>
#include <wx/settings.h>
#include <wx/tooltip.h>

// IMPLEMENT_APP must be in global scope
IMPLEMENT_APP(OpenGlass::OpenGlassApp)

namespace OpenGlass
{
	void OpenGlassApp::OnInitCmdLine(wxCmdLineParser& parser)
	{
		wxApp::OnInitCmdLine(parser);
		parser.AddLongOption(
			L"elevated-pipe",
			L"internal elevation handshake pipe",
			wxCMD_LINE_VAL_STRING,
			wxCMD_LINE_HIDDEN
		);
	}

	bool OpenGlassApp::OnInit()
	{
		// Override the default GUI font before any window is created.
		// The stock font (Segoe UI 9pt) has no CJK optimization and falls back to
		// SimSun via font linking in a Chinese locale, which renders poorly.
		// Microsoft YaHei UI provides proper CJK + Latin mixing for the localized UI.
		if (wxFont guiFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT); guiFont.IsOk())
		{
			guiFont.SetFaceName(L"Microsoft YaHei UI");
			if (guiFont.GetPointSize() >= 9)
			{
				guiFont.SetPointSize(guiFont.GetPointSize() + 1);
			}
			if (guiFont.IsOk())
			{
				wxSystemSettings::SetFont(wxSYS_DEFAULT_GUI_FONT, guiFont);
			}
		}

		// Localized tooltips are long-form Chinese text; keep them on screen
		// longer than the 5-second default so they can actually be read.
		wxToolTip::SetAutoPop(20000);
		wxToolTip::SetDelay(300);

		MSWEnableDarkMode(wxApp::DarkMode_Auto);
		if (!wxApp::OnInit())
			return false;

		const auto startup = Elevation::PrepareElevatedStartup();
		if (!startup.continueStartup)
		{
			return false;
		}

		// Machine settings and the schema migration are shared; do not allow two
		// target-user editors to race in the same interactive session.
		m_singleInstanceChecker.Create(L"OpenGlassGUI.SingleInstance");
		if (m_singleInstanceChecker.IsAnotherRunning())
			return false;

		if (!ConfigurationMigration::EnsureCanonicalConfiguration(startup.userSid))
			return false;

		MainFrame* frame = new MainFrame(L"Aero Glass for Win10+", startup.userSid);
		if (frame->IsInitializationCanceled())
		{
			frame->Destroy();
			return false;
		}
		frame->Show(true);
		return true;
	}
}
