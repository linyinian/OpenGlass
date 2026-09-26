#include "pch.h"
#include "OpenGlassGUI.hpp"
#include "MainFrame.hpp"
#include "Elevation.hpp"
#include "ConfigurationMigration.hpp"
#include <wx/cmdline.h>
#include <wx/settings.h>
#include <wx/tooltip.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

// IMPLEMENT_APP must be in global scope
IMPLEMENT_APP(OpenGlass::OpenGlassApp)

namespace OpenGlass
{
	namespace
	{
			// Win11 window effects: rounded corners (always works) and the Mica
		// backdrop (22H2+, best effort). Mica only shows through frame padding and
		// unpainted regions; child controls keep their own backgrounds.
		// All calls are non-fatal: on failure the window simply keeps the classic look.
		void EnableWin11WindowEffects(wxFrame* frame)
		{
		const HWND hwnd = frame->GetHWND();

		// 33/ROUND and 38/MICA are Win11 DWM attributes; numeric values with a
		// cast so this compiles regardless of how the SDK declares the enums.
		DWM_WINDOW_CORNER_PREFERENCE corner = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(2); // DWMWCP_ROUND
		DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));

		DWM_SYSTEMBACKDROP_TYPE backdrop = static_cast<DWM_SYSTEMBACKDROP_TYPE>(2); // DWMSBT_MICA
		if (SUCCEEDED(DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof(backdrop))))
			{
				const MARGINS sheetMargins{-1, -1, -1, -1};
				if (SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &sheetMargins)))
				{
				// Stop wx from painting an opaque background over the backdrop.
				frame->SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
				frame->SetBackgroundColour(wxNullColour);
				frame->Refresh();
				}
			}
		}

		// Apply the font to a window and, recursively, to all of its children.
		// wxWindow::GetFont() bubbles up to the parent, so setting it on the frame
		// also covers children created later (dialogs with this frame as parent,
		// controls added at runtime); this pass covers everything built up front.
		// Also localizes the stock "Browse" label used by wxFilePickerCtrl/wxDirPickerCtrl.
		void ApplyFontToWindowTree(wxWindow* window, const wxFont& font)
		{
			window->SetFont(font);
			if (auto* button = wxDynamicCast(window, wxButton))
			{
				const wxString label = button->GetLabel();
				if (label == L"Browse" || label == L"Browse..." || label == L"...")
				{
					button->SetLabel(L"浏览...");
				}
			}
			for (wxWindow* child : window->GetChildren())
			{
				ApplyFontToWindowTree(child, font);
			}
		}
	}

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
		// The stock GUI font (Segoe UI 9pt) has no CJK optimization and falls back
		// to SimSun via font linking in a Chinese locale, which renders poorly.
		// Build a Microsoft YaHei UI font for proper CJK + Latin mixing.
		wxFont localizedFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
		if (localizedFont.IsOk())
		{
			localizedFont.SetFaceName(L"Microsoft YaHei UI");
			if (localizedFont.GetPointSize() >= 9)
			{
				localizedFont.SetPointSize(localizedFont.GetPointSize() + 1);
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
		if (localizedFont.IsOk())
		{
			ApplyFontToWindowTree(frame, localizedFont);
		}
		EnableWin11WindowEffects(frame);
		frame->Show(true);
		return true;
	}
}
