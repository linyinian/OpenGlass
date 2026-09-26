#include "pch.h"
#include "MainFrame.hpp"
#include "ColorSwatchButton.hpp"
#include "Symbols.hpp"
#include "UiControls.hpp"
#include "BlurSettings.hpp"

namespace OpenGlass
{
	namespace
	{
		wxCollapsiblePane* AddCollapsibleSection(
			wxScrolledWindow* panel,
			wxSizer* parentSizer,
			const wxString& label
		)
		{
			auto* pane = new wxCollapsiblePane(
				panel,
				wxID_ANY,
				label,
				wxDefaultPosition,
				wxDefaultSize,
				wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE
			);
			pane->Collapse(true);
			if (wxControl* header = pane->GetControlWidget())
			{
				if (wxSizer* headerSizer = header->GetContainingSizer())
				{
					if (wxSizerItem* headerItem = headerSizer->GetItem(header))
					{
						headerItem->SetFlag(headerItem->GetFlag() & ~wxLEFT);
						pane->InvalidateBestSize();
					}
				}
			}
			parentSizer->Add(pane, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 5);
			pane->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [panel](wxCollapsiblePaneEvent&)
			{
				panel->Layout();
				panel->FitInside();
			});
			return pane;
		}

		void WrapStaticTextToParentWidth(wxStaticText* label, const wxString& sourceText, int rightPadding = 8)
		{
			if (!label)
			{
				return;
			}

			wxWindow* parent = label->GetParent();
			if (!parent)
			{
				return;
			}

			const int availableWidth = std::max(1, parent->GetClientSize().GetWidth() - label->GetPosition().x - rightPadding);
			label->SetLabel(sourceText);
			label->Wrap(availableWidth);
		}

		void AddAdminRequiredTip(wxPanel* panel, wxBoxSizer* sizer, const wxString& message)
		{
			if (!panel || !sizer)
			{
				return;
			}

			auto* tipPanel = new wxPanel(panel);
			tipPanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));

			auto* tipSizer = new wxBoxSizer(wxHORIZONTAL);
			auto* icon = new wxStaticBitmap(
				tipPanel,
				wxID_ANY,
				wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16))
			);
			tipSizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

			auto* label = new wxStaticText(tipPanel, wxID_ANY, message);
			label->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
			tipSizer->Add(label, 1, wxEXPAND);

			tipPanel->SetSizer(tipSizer);
			tipPanel->Bind(wxEVT_SIZE, [label, message](wxSizeEvent& event)
			{
				WrapStaticTextToParentWidth(label, message);
				event.Skip();
			});
			tipPanel->CallAfter([tipPanel, label, message]
			{
				tipPanel->Layout();
				WrapStaticTextToParentWidth(label, message);
			});
			sizer->Insert(0, tipPanel, 0, wxEXPAND | wxALL, 8);

			for (wxWindowList::compatibility_iterator node = panel->GetChildren().GetFirst(); node; node = node->GetNext())
			{
				wxWindow* child = node->GetData();
				if (child != tipPanel)
				{
					child->Enable(false);
				}
			}
		}
	}

	void MainFrame::CreateSystemTab()
	{
		wxPanel* panel = new wxPanel(m_notebook);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		
		wxStaticBoxSizer* globalGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"全局");
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkDisableGlassOnBattery = new wxCheckBox(panel, wxID_ANY, L"省电时禁用透明效果");
			row->Add(m_chkDisableGlassOnBattery, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisableGlassOnBattery);
			globalGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		m_chkDisableGlassOnBattery->SetToolTip(L"勾选后，当节能/省电模式开启时，毛玻璃效果将变为不透明。");
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkGlassSafetyZone = new wxCheckBox(panel, wxID_ANY, L"禁用毛玻璃安全区");
			row->Add(m_chkGlassSafetyZone, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::GlassSafetyZoneMode);
			globalGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		m_chkGlassSafetyZone->SetToolTip(L"禁用此选项可能导致视觉瑕疵。默认启用（不勾选）。");

		// Disabled Hooks
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(panel, wxID_ANY, L"已禁用的钩子（高级）：*"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisabledHooks);
			globalGroup->Add(row, 0, wxLEFT | wxTOP, 2);
		}
		wxArrayString hooks;
		hooks.Add(L"标题栏文字处理器");    // 0x1
		hooks.Add(L"强调色覆盖器");        // 0x2
		hooks.Add(L"毛玻璃边框处理器");     // 0x4
		hooks.Add(L"毛玻璃反射处理器");// 0x8
		hooks.Add(L"标题栏尺寸调整器");// 0x10
		m_clDisabledHooks = new wxCheckListBox(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, hooks);
		globalGroup->Add(m_clDisabledHooks, 0, wxEXPAND | wxALL, 4);
		m_clDisabledHooks->SetToolTip(L"控制禁用哪些模块的钩子。\n非兼容性维护请勿修改。");

		sizer->Add(globalGroup, 0, wxEXPAND | wxALL, 4);
		if (!m_isAdmin)
		{
			AddAdminRequiredTip(panel, sizer, L"编辑全局设置需要管理员权限。");
		}

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"系统");
	}

	void MainFrame::CreateDiagnosticsTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		wxStaticBoxSizer* transparencyGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"透明状态");
		const wxString transparencyDescriptionText = L"当前可能导致毛玻璃不透明的条件。";
		wxStaticText* transparencyDescription = new wxStaticText(
			panel,
			wxID_ANY,
			transparencyDescriptionText
		);
		transparencyGroup->Add(transparencyDescription, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		wxFlexGridSizer* transparencyGrid = new wxFlexGridSizer(2, 8, 12);
		transparencyGrid->AddGrowableCol(1, 1);
		auto addTransparencyRow = [&](const wxString& label, wxStaticText*& value)
		{
			wxStaticText* key = new wxStaticText(panel, wxID_ANY, label);
			wxFont keyFont = key->GetFont();
			keyFont.SetWeight(wxFONTWEIGHT_BOLD);
			key->SetFont(keyFont);
			transparencyGrid->Add(key, 0, wxALIGN_TOP);
			value = new wxStaticText(panel, wxID_ANY, L"读取中...");
			transparencyGrid->Add(value, 1, wxEXPAND);
		};
		addTransparencyRow(L"Windows 透明效果", m_lblWindowsTransparencyStatus);
		addTransparencyRow(L"不透明混合", m_lblOpaqueBlendStatus);
		addTransparencyRow(L"电源模式", m_lblPowerModeStatus);
		addTransparencyRow(L"省电时不透明", m_lblDisableOnBatteryStatus);
		wxStaticText* resultKey = new wxStaticText(panel, wxID_ANY, L"结果");
		wxFont resultKeyFont = resultKey->GetFont();
		resultKeyFont.SetWeight(wxFONTWEIGHT_BOLD);
		resultKey->SetFont(resultKeyFont);
		transparencyGrid->Add(resultKey, 0, wxALIGN_TOP);
		wxBoxSizer* transparencyResultRow = new wxBoxSizer(wxHORIZONTAL);
		m_bmpEffectiveTransparencyWarning = new wxStaticBitmap(
			panel,
			wxID_ANY,
			wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16))
		);
		m_bmpEffectiveTransparencyWarning->Hide();
		transparencyResultRow->Add(m_bmpEffectiveTransparencyWarning, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
		m_lblEffectiveTransparencyStatus = new wxStaticText(panel, wxID_ANY, L"读取中...");
		transparencyResultRow->Add(m_lblEffectiveTransparencyStatus, 1, wxEXPAND);
		transparencyGrid->Add(transparencyResultRow, 1, wxEXPAND);
		transparencyGroup->Add(transparencyGrid, 0, wxEXPAND | wxALL, 8);

		wxBoxSizer* transparencyButtonRow = new wxBoxSizer(wxHORIZONTAL);
		transparencyButtonRow->AddStretchSpacer();
		m_btnRefreshTransparencyDiagnostics = new wxButton(panel, wxID_ANY, L"刷新");
		transparencyButtonRow->Add(m_btnRefreshTransparencyDiagnostics, 0);
		transparencyGroup->Add(transparencyButtonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* downloadGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"符号");
		const wxString descriptionText =
			L"仅当 OpenGlass 提示无法为当前的 uDWM.dll 或 dwmcore.dll 自动下载符号时才使用此功能。"
			L"它只是将精确匹配的公共 PDB 文件预取到缓存中；不会修复 DWM 崩溃、更改渲染或执行任何其他修复。";
		wxStaticText* description = new wxStaticText(
			panel,
			wxID_ANY,
			descriptionText
		);
		downloadGroup->Add(description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		auto* symbolNoticePanel = new wxPanel(panel);
		symbolNoticePanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
		auto* symbolNoticeSizer = new wxBoxSizer(wxHORIZONTAL);
		auto* symbolNoticeIcon = new wxStaticBitmap(
			symbolNoticePanel,
			wxID_ANY,
			wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16))
		);
		symbolNoticeSizer->Add(symbolNoticeIcon, 0, wxALIGN_TOP | wxRIGHT, 8);
		auto* symbolNoticeContent = new wxBoxSizer(wxVERTICAL);
		const wxString symbolNoticeText =
			L"如果遇到 DWM 崩溃，请携带确切的 Windows 内部版本号、修订号和完整转储，及时在 GitHub 提交 issue。"
			L"Reddit、Discord 及其他第三方渠道的帖子不会被作为 OpenGlass 的 bug 报告跟踪。";
		auto* symbolNoticeLabel = new wxStaticText(symbolNoticePanel, wxID_ANY, symbolNoticeText);
		symbolNoticeLabel->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
		symbolNoticeContent->Add(symbolNoticeLabel, 0, wxEXPAND | wxBOTTOM, 4);
		auto* symbolIssueLink = new wxHyperlinkCtrl(
			symbolNoticePanel,
			wxID_ANY,
			L"在 GitHub 提交 issue",
			L"https://github.com/ALTaleX531/OpenGlass/issues/new"
		);
		symbolIssueLink->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));
		symbolNoticeContent->Add(symbolIssueLink, 0);
		symbolNoticeSizer->Add(symbolNoticeContent, 1, wxEXPAND);
		symbolNoticePanel->SetSizer(symbolNoticeSizer);
		symbolNoticePanel->Bind(wxEVT_SIZE, [symbolNoticeLabel, symbolNoticeText](wxSizeEvent& event)
		{
			WrapStaticTextToParentWidth(symbolNoticeLabel, symbolNoticeText);
			event.Skip();
		});
		symbolNoticePanel->CallAfter([symbolNoticePanel, symbolNoticeLabel, symbolNoticeText]
		{
			symbolNoticePanel->Layout();
			WrapStaticTextToParentWidth(symbolNoticeLabel, symbolNoticeText);
		});
		downloadGroup->Add(symbolNoticePanel, 0, wxEXPAND | wxALL, 8);

		wxFlexGridSizer* infoGrid = new wxFlexGridSizer(2, 8, 12);
		infoGrid->AddGrowableCol(1, 1);
		auto addInfoRow = [&](const wxString& label, wxWindow* value)
		{
			wxStaticText* key = new wxStaticText(panel, wxID_ANY, label);
			wxFont keyFont = key->GetFont();
			keyFont.SetWeight(wxFONTWEIGHT_BOLD);
			key->SetFont(keyFont);
			infoGrid->Add(key, 0, wxALIGN_TOP);
			infoGrid->Add(value, 1, wxEXPAND);
		};

		wxStaticText* modulesValue = new wxStaticText(panel, wxID_ANY, L"uDWM.dll, dwmcore.dll");
		addInfoRow(L"模块", modulesValue);

		m_dpSymbolCacheDirectory = new wxDirPickerCtrl(
			panel,
			wxID_ANY,
			GetSymbolCacheDirectory(),
			L"选择符号缓存文件夹",
			wxDefaultPosition,
			wxDefaultSize,
			wxDIRP_USE_TEXTCTRL
		);
		m_dpSymbolCacheDirectory->Enable(m_isAdmin);
		m_dpSymbolCacheDirectory->SetToolTip(L"选择下载的 PDB 文件的存放位置。文件夹会在需要时自动创建。");
		addInfoRow(L"缓存路径", m_dpSymbolCacheDirectory);

		wxStaticText* timeoutValue = new wxStaticText(
			panel,
			wxID_ANY,
			wxString::Format(L"每请求 %d 秒", SymbolDownloadTimeoutSeconds)
		);
		addInfoRow(L"网络超时", timeoutValue);
		downloadGroup->Add(infoGrid, 0, wxEXPAND | wxALL, 8);

		wxBoxSizer* buttonRow = new wxBoxSizer(wxHORIZONTAL);
		buttonRow->AddStretchSpacer();
		m_btnDownloadSymbols = new wxButton(
			panel,
			wxID_ANY,
			L"下载符号"
		);
		buttonRow->Add(m_btnDownloadSymbols, 0, wxRIGHT, 6);

		m_btnCancelSymbolDownload = new wxButton(panel, wxID_ANY, L"取消");
		m_btnCancelSymbolDownload->Enable(false);
		buttonRow->Add(m_btnCancelSymbolDownload, 0);
		downloadGroup->Add(buttonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* statusGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"符号下载状态");
		m_gaugeSymbolDownload = new wxGauge(panel, wxID_ANY, 100);
		statusGroup->Add(m_gaugeSymbolDownload, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		m_lblSymbolDownloadPhase = new wxStaticText(panel, wxID_ANY, L"空闲");
		wxFont phaseFont = m_lblSymbolDownloadPhase->GetFont();
		phaseFont.SetWeight(wxFONTWEIGHT_BOLD);
		m_lblSymbolDownloadPhase->SetFont(phaseFont);
		statusGroup->Add(m_lblSymbolDownloadPhase, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		m_symbolDownloadDetailText = L"准备下载 uDWM.dll 和 dwmcore.dll。";
		m_lblSymbolDownloadDetail = new wxStaticText(panel, wxID_ANY, m_symbolDownloadDetailText);
		statusGroup->Add(m_lblSymbolDownloadDetail, 0, wxEXPAND | wxALL, 8);

		m_pnlSymbolDownloadResult = new wxPanel(panel);
		wxBoxSizer* resultRow = new wxBoxSizer(wxHORIZONTAL);
		m_bmpSymbolDownloadResult = new wxStaticBitmap(
			m_pnlSymbolDownloadResult,
			wxID_ANY,
			wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, wxSize(16, 16))
		);
		m_bmpSymbolDownloadResult->SetMinSize(wxSize(16, 16));
		resultRow->Add(m_bmpSymbolDownloadResult, 0, wxALIGN_TOP | wxRIGHT, 8);

		m_lblSymbolDownloadResult = new wxStaticText(m_pnlSymbolDownloadResult, wxID_ANY, wxEmptyString);
		resultRow->Add(m_lblSymbolDownloadResult, 1, wxEXPAND);

		m_pnlSymbolDownloadResult->SetSizer(resultRow);
		m_pnlSymbolDownloadResult->Hide();
		statusGroup->Add(m_pnlSymbolDownloadResult, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* dumpGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"WER 崩溃转储");
		const wxString dumpDescriptionText = L"通过 dwm.exe 的按应用 Windows 错误报告（WER）配置收集完整的用户态崩溃转储。完整转储可能较大。默认文件夹为 %ProgramData%\\OpenGlass\\dumps。";
		wxStaticText* dumpDescription = new wxStaticText(panel, wxID_ANY, dumpDescriptionText);
		dumpGroup->Add(dumpDescription, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		wxFlexGridSizer* dumpGrid = new wxFlexGridSizer(2, 8, 12);
		dumpGrid->AddGrowableCol(1, 1);
		wxStaticText* dumpFolderLabel = new wxStaticText(panel, wxID_ANY, L"转储文件夹");
		wxFont dumpFolderFont = dumpFolderLabel->GetFont();
		dumpFolderFont.SetWeight(wxFONTWEIGHT_BOLD);
		dumpFolderLabel->SetFont(dumpFolderFont);
		dumpGrid->Add(dumpFolderLabel, 0, wxALIGN_CENTER_VERTICAL);
		m_dpDwmCrashDumpFolder = new wxDirPickerCtrl(
			panel,
			wxID_ANY,
			GetDefaultDwmCrashDumpFolder(),
			L"选择 DWM 崩溃转储文件夹",
			wxDefaultPosition,
			wxDefaultSize,
			wxDIRP_USE_TEXTCTRL
		);
		m_dpDwmCrashDumpFolder->Enable(m_isAdmin);
		m_dpDwmCrashDumpFolder->SetToolTip(L"文件夹 ACL 必须允许 WER 为崩溃的 DWM 进程收集转储。相对路径将相对于 OpenGlassGUI.exe 解析。");
		dumpGrid->Add(m_dpDwmCrashDumpFolder, 1, wxEXPAND);
		dumpGroup->Add(dumpGrid, 0, wxEXPAND | wxALL, 8);

		m_dwmCrashDumpStatusText = L"正在读取当前 dwm.exe 转储配置...";
		m_lblDwmCrashDumpStatus = new wxStaticText(panel, wxID_ANY, m_dwmCrashDumpStatusText);
		dumpGroup->Add(m_lblDwmCrashDumpStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxBoxSizer* dumpButtonRow = new wxBoxSizer(wxHORIZONTAL);
		dumpButtonRow->AddStretchSpacer();
		m_btnEnableDwmCrashDumps = new wxButton(panel, wxID_ANY, L"启用完整转储");
		m_btnEnableDwmCrashDumps->Enable(m_isAdmin);
		dumpButtonRow->Add(m_btnEnableDwmCrashDumps, 0, wxRIGHT, 6);
		m_btnDisableDwmCrashDumps = new wxButton(panel, wxID_ANY, L"禁用转储");
		m_btnDisableDwmCrashDumps->Enable(false);
		m_btnDisableDwmCrashDumps->SetToolTip(L"移除按应用的 LocalDumps\\dwm.exe 注册表键。系统级 WER 设置不受影响。");
		dumpButtonRow->Add(m_btnDisableDwmCrashDumps, 0);
		dumpGroup->Add(dumpButtonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		sizer->Add(transparencyGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		sizer->Add(downloadGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		sizer->Add(statusGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		sizer->Add(dumpGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		sizer->AddStretchSpacer();
		if (!m_isAdmin)
		{
			AddAdminRequiredTip(panel, sizer, L"下载符号或修改 WER 崩溃转储设置需要管理员权限。");
		}

		panel->SetSizer(sizer);
		panel->Bind(wxEVT_SIZE, [this, panel, transparencyDescription, transparencyDescriptionText, description, descriptionText, dumpDescription, dumpDescriptionText](wxSizeEvent& event)
		{
			WrapStaticTextToParentWidth(transparencyDescription, transparencyDescriptionText);
			WrapStaticTextToParentWidth(description, descriptionText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
			WrapStaticTextToParentWidth(dumpDescription, dumpDescriptionText);
			WrapStaticTextToParentWidth(m_lblDwmCrashDumpStatus, m_dwmCrashDumpStatusText);
			panel->FitInside();
			event.Skip();
		});
		panel->CallAfter([this, panel, transparencyDescription, transparencyDescriptionText, description, descriptionText, dumpDescription, dumpDescriptionText]
		{
			panel->Layout();
			WrapStaticTextToParentWidth(transparencyDescription, transparencyDescriptionText);
			WrapStaticTextToParentWidth(description, descriptionText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
			WrapStaticTextToParentWidth(dumpDescription, dumpDescriptionText);
			WrapStaticTextToParentWidth(m_lblDwmCrashDumpStatus, m_dwmCrashDumpStatusText);
			panel->Layout();
			panel->FitInside();
		});
		RefreshTransparencyDiagnostics();
		RefreshDwmCrashDumpConfiguration();
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
		m_notebook->AddPage(panel, L"诊断");
	}

	void MainFrame::CreateThemeTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		// Theme Resources Group
		wxStaticBoxSizer* themeGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"主题");

		// Custom Theme Atlas Row
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeAtlas = new wxCheckBox(panel, wxID_ANY, L"主题图集图片");
			m_chkCustomThemeAtlas->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeAtlas, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			m_fpCustomThemeAtlas = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"选择主题图集", L"PNG 图片 (*.png)|*.png", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeAtlas, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeAtlas);
			AddPathWarningIcon(panel, row, m_fpCustomThemeAtlas, m_chkCustomThemeAtlas, L"主题图集");
			themeGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		// Export Button
		m_btnExportAtlas = new wxButton(panel, wxID_ANY, L"导出当前主题图集");
		themeGroup->Add(m_btnExportAtlas, 0, wxALL, 4);

		sizer->Add(themeGroup, 0, wxEXPAND | wxALL, 4);

		// Reflection Group
		wxStaticBoxSizer* reflectionGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"反射");

		// Custom Reflection Row
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeReflection = new wxCheckBox(panel, wxID_ANY, L"毛玻璃反射图片");
			m_chkCustomThemeReflection->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeReflection, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_fpCustomThemeReflection = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"选择反射纹理", L"图片文件 (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeReflection, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeReflection);
			AddPathWarningIcon(panel, row, m_fpCustomThemeReflection, m_chkCustomThemeReflection, L"反射纹理");
			reflectionGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		// Reflection Intensity
		m_slReflectionIntensity = new NativeSlider(panel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(panel, reflectionGroup, L"毛玻璃反射强度：", m_slReflectionIntensity, Settings::Id::ColorizationGlassReflectionIntensity);

		// Reflection Parallax
		m_slReflectionParallax = new NativeSlider(panel, wxID_ANY, 13, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(panel, reflectionGroup, L"毛玻璃视差强度：", m_slReflectionParallax, Settings::Id::ColorizationGlassReflectionParallaxIntensity);

		// Reflection Policy
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(panel, wxID_ANY, L"毛玻璃反射策略：", wxDefaultPosition, wxSize(300, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			m_chkReflectionPolicyTitlebar = new wxCheckBox(panel, wxID_ANY, L"标题栏");
			row->Add(m_chkReflectionPolicyTitlebar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
			m_chkReflectionPolicyPeek = new wxCheckBox(panel, wxID_ANY, L"Aero Peek");
			row->Add(m_chkReflectionPolicyPeek, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
			m_chkReflectionPolicySnap = new wxCheckBox(panel, wxID_ANY, L"Aero Snap");
			row->Add(m_chkReflectionPolicySnap, 0, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::ColorizationGlassReflectionPolicy);
			reflectionGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		// Legacy (kept for reference):
		// wxArrayString policies;
		// policies.Add(L"标题栏");    // 1<<0
		// policies.Add(L"Aero Peek");   // 1<<2
		// policies.Add(L"Aero Snap");   // 1<<3
		// m_clReflectionPolicy = new wxCheckListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 60), policies);
		// AddProperty(panel, reflectionGroup, L"毛玻璃反射策略：", m_clReflectionPolicy, L"ColorizationGlassReflectionPolicy");
		
		// Reflection Opacity & Variants
		auto* reflectionOpacityPane = AddCollapsibleSection(
			panel,
			reflectionGroup,
			L"高级不透明度设置"
		);
		wxWindow* reflectionOpacityPanel = reflectionOpacityPane->GetPane();
		auto* reflectionOpacitySizer = new wxBoxSizer(wxVERTICAL);

		auto addRefOpacity = [&](const wxString& label, wxChoice*& ch, wxSlider*& sl) {
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* st = new wxStaticText(reflectionOpacityPanel, wxID_ANY, label, wxDefaultPosition, wxSize(300, -1)); // Aligned width
			row->Add(st, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString choices;
			choices.Add(L"自动");
			choices.Add(L"跟随主题");
			choices.Add(L"自定义");
			ch = new wxChoice(reflectionOpacityPanel, wxID_ANY, wxDefaultPosition, wxSize(120, -1), choices);
			ch->SetSelection(0);
			row->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			sl = new NativeSlider(reflectionOpacityPanel, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
			sl->Disable();
			row->Add(sl, 1, wxALIGN_CENTER_VERTICAL); // Flex 1 for slider

			reflectionOpacitySizer->Add(row, 0, wxEXPAND | wxALL, 4);
		};

		addRefOpacity(L"基础不透明度：", m_chModeReflectionOpacity, m_slReflectionOpacity);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacity);
		addRefOpacity(L"非活动状态基础不透明度：", m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityInactive);
		addRefOpacity(L"最大化时基础不透明度：", m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityMaximized);
		addRefOpacity(L"非活动最大化时基础不透明度：", m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized);
		reflectionOpacityPanel->SetSizer(reflectionOpacitySizer);

		sizer->Add(reflectionGroup, 0, wxEXPAND | wxALL, 4);

		// Material Group
		wxStaticBoxSizer* materialGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"材质");

		// CustomThemeMaterial
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeMaterial = new wxCheckBox(panel, wxID_ANY, L"自定义材质纹理");
			m_chkCustomThemeMaterial->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeMaterial, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_fpCustomThemeMaterial = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"选择材质纹理", L"图片文件 (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeMaterial, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeMaterial);
			AddPathWarningIcon(panel, row, m_fpCustomThemeMaterial, m_chkCustomThemeMaterial, L"材质纹理");
			materialGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		// Material Opacity
		m_slMaterialOpacity = new NativeSlider(panel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(panel, materialGroup, L"材质不透明度：", m_slMaterialOpacity, Settings::Id::MaterialOpacity);

		sizer->Add(materialGroup, 0, wxEXPAND | wxALL, 4);

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"主题");
	}

	void MainFrame::CreateAppearanceTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		// General Group
		wxStaticBoxSizer* generalGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"常规");
		
		// CCustomBlur's projected BlurAmount. BlurDeviation is its historical registry encoding.
		m_slBlurAmount = new NativeSlider(
			panel,
			wxID_ANY,
			BlurSettings::DecodeGuiBlurAmount(BlurSettings::DefaultEncodedDeviation),
			BlurSettings::GuiMinimumBlurAmount,
			BlurSettings::GuiMaximumBlurAmount,
			wxDefaultPosition,
			wxDefaultSize,
			wxSL_HORIZONTAL
		);
		AddProperty(panel, generalGroup, L"模糊数量：", m_slBlurAmount, Settings::Id::BlurDeviation);
		// Blur Optimization
		wxArrayString blurOpts;
		blurOpts.Add(L"速度优先");
		blurOpts.Add(L"均衡");
		blurOpts.Add(L"质量优先");
		m_chBlurOptimization = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, blurOpts);
		AddProperty(panel, generalGroup, L"模糊优化：", m_chBlurOptimization, Settings::Id::BlurOptimization);

		// Renderer Settings
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkUseD3D = new wxCheckBox(panel, wxID_ANY, L"使用 Direct3D 渲染器");
			row->Add(m_chkUseD3D, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::UseDirect3DRendering);
			generalGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		m_chkUseD3D->SetToolTip(L"使用 Direct3D 11 后端。它会忽略 BlurDeviation 和 BlurOptimization，改用固定的 3 px 高斯标准差。");

		sizer->Add(generalGroup, 0, wxEXPAND | wxALL, 4);
		// Window Group
		wxStaticBoxSizer* geometryGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"窗口");

		// Round Rect Radius
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"毛玻璃几何圆角半径：", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString profiles;
			profiles.Add(L"Windows 8 风格（原生）");
			profiles.Add(L"Windows 7 风格");
			profiles.Add(L"自定义");
			m_chRoundRectProfile = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, profiles);
			row->Add(m_chRoundRectProfile, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_scRoundRectRadius = new wxSpinCtrl(panel, wxID_ANY);
			m_scRoundRectRadius->SetRange(0, 50);
			row->Add(m_scRoundRectRadius, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::RoundRectRadius);
			geometryGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		// Caption Buttons
		wxArrayString captionStyles;
		captionStyles.Add(L"原生风格");
		captionStyles.Add(L"Windows Vista 风格");
		captionStyles.Add(L"Windows 7 风格");
		captionStyles.Add(L"Windows 8 风格");
		m_chCaptionButtons = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, captionStyles);
		AddProperty(panel, geometryGroup, L"标题栏按钮样式：", m_chCaptionButtons, Settings::Id::CaptionButtons);

		// Disable Modern Borders
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkDisableModernBorders = new wxCheckBox(panel, wxID_ANY, L"禁用现代边框");
			row->Add(m_chkDisableModernBorders, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisableModernBorders);
			geometryGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		sizer->Add(geometryGroup, 0, wxEXPAND | wxALL, 4);

		// Caption Group
		wxStaticBoxSizer* textGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"标题栏");

		// Center Caption
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"文字居中：", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString modes;
			modes.Add(L"禁用（原生）");
			modes.Add(L"常规");
			modes.Add(L"Windows 8 风格");
			m_chCenterCaption = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			row->Add(m_chCenterCaption, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::CenterCaption);
			textGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		// Text Glow Mode
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"文字发光模式：", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString glowModes;
			glowModes.Add(L"无发光");
			glowModes.Add(L"使用主题图集");
			glowModes.Add(L"使用主题图集（不透明度）");
			glowModes.Add(L"合成（主题设置）");
			m_chTextGlowMode = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, glowModes);
			row->Add(m_chTextGlowMode, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			row->Add(new wxStaticText(panel, wxID_ANY, L"大小："), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
			m_scTextGlowSize = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1));
			m_scTextGlowSize->SetRange(0, 100);
			row->Add(m_scTextGlowSize, 0, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::TextGlowMode);

			textGroup->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		
		// Text color overrides
		auto* textColorOverridesPane = AddCollapsibleSection(
			panel,
			textGroup,
			L"文字颜色覆盖"
		);
		wxWindow* textColorOverridesPanel = textColorOverridesPane->GetPane();
		auto* textColorOverridesSizer = new wxBoxSizer(wxVERTICAL);

		// Helper to add choice + picker
		auto addColorOverride = [&](wxChoice*& ch, wxColourPickerCtrl*& cp, const wxString& label) {
			wxBoxSizer* r = new wxBoxSizer(wxHORIZONTAL);
			r->Add(new wxStaticText(textColorOverridesPanel, wxID_ANY, label, wxDefaultPosition, wxSize(180, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString modes;
			modes.Add(L"自动");
			modes.Add(L"跟随主题");
			modes.Add(L"自定义");
			modes.Add(L"由系统决定");
			ch = new wxChoice(textColorOverridesPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			r->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			cp = new wxColourPickerCtrl(textColorOverridesPanel, wxID_ANY);
			cp->Enable(false); // Default disabled
			r->Add(cp, 1, wxALIGN_CENTER_VERTICAL);
			textColorOverridesSizer->Add(r, 0, wxEXPAND | wxALL, 4);
		};

		addColorOverride(m_chModeColorCaption, m_cpColorCaption, L"活动：");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaption);
		addColorOverride(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, L"非活动：");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionInactive);
		addColorOverride(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, L"活动最大化：");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionMaximized);
		addColorOverride(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, L"非活动最大化：");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionInactiveMaximized);
		textColorOverridesPanel->SetSizer(textColorOverridesSizer);

		sizer->Add(textGroup, 0, wxEXPAND | wxALL, 4);

		wxStaticBoxSizer* accentGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"强调色");
		wxBoxSizer* accentRow = new wxBoxSizer(wxHORIZONTAL);
		m_chkGlassOverrideAccent = new wxCheckBox(panel, wxID_ANY, L"覆盖强调色");
		m_chkGlassOverrideAccent->SetToolTip(L"用 OpenGlass 效果覆盖强调色模糊表面。");
		accentRow->Add(m_chkGlassOverrideAccent, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
		AddOptionStatus(panel, accentRow, Settings::Id::GlassOverrideAccent);
		accentGroup->Add(accentRow, 0, wxEXPAND | wxALL, 4);
		sizer->Add(accentGroup, 0, wxEXPAND | wxALL, 4);

		panel->SetSizer(sizer);
		m_notebook->AddPage(panel, L"外观");
	}

	void MainFrame::CreateGlassColorsTab()
	{
		// Use ScrolledWindow for large content
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		m_glassColorsPanel = panel;
		m_glassColorsRootSizer = sizer;

		// Glass Type
		wxArrayString glassTypes;
		glassTypes.Add(L"Windows Vista Aero");
		glassTypes.Add(L"Windows 7 Aero");
		m_rbGlassType = new wxRadioBox(panel, wxID_ANY, L"毛玻璃类型", wxDefaultPosition, wxDefaultSize, glassTypes, 2, wxRA_SPECIFY_COLS);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(m_rbGlassType, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::GlassType);
			sizer->Add(row, 0, wxALL, 5);
		}

		// Built-in Vista and Windows 7 color presets. Both groups are created once;
		// UpdateUIVisibility shows only the family selected above.
		m_colorPresetsGroupSizer = new wxStaticBoxSizer(wxVERTICAL, panel, L"颜色预设");
		m_vistaPresetSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
		m_windows7PresetSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);

		auto addPresetButtons = [this, panel](
			wxWrapSizer* presetSizer,
			std::span<const ColorizationPresets::Preset> presets
		) {
			for (const auto& preset : presets)
			{
				const wxString label{ preset.name.data(), preset.name.size() };
				auto* button = new ColorSwatchButton(
					panel,
					wxID_ANY,
					label,
					preset.argb
				);
				auto* caption = new wxStaticText(
					panel,
					wxID_ANY,
					label,
					wxDefaultPosition,
					FromDIP(wxSize(64, -1)),
					wxALIGN_CENTER_HORIZONTAL
				);
				auto* cell = new wxBoxSizer(wxVERTICAL);
				cell->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
				cell->Add(caption, 0, wxEXPAND | wxTOP, 2);
				presetSizer->Add(cell, 0, wxALL, 1);
				m_presetButtons.emplace_back(&preset, button);
			}
		};

		addPresetButtons(m_vistaPresetSizer, ColorizationPresets::Get(ColorizationPresets::Family::Vista));
		addPresetButtons(m_windows7PresetSizer, ColorizationPresets::Get(ColorizationPresets::Family::Windows7));
		auto addCustomColorButton = [this, panel](wxWrapSizer* presetSizer) {
			constexpr DWORD InitialColor = 0xFF000000;
			auto* button = new ColorSwatchButton(panel, wxID_ANY, L"自定义", InitialColor);
			auto* caption = new wxStaticText(
				panel,
				wxID_ANY,
				L"自定义",
				wxDefaultPosition,
				FromDIP(wxSize(64, -1)),
				wxALIGN_CENTER_HORIZONTAL
			);
			auto* cell = new wxBoxSizer(wxVERTICAL);
			cell->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
			cell->Add(caption, 0, wxEXPAND | wxTOP, 2);
			presetSizer->Add(cell, 0, wxALL, 1);
			m_customColorButtons.push_back(button);
		};
		addCustomColorButton(m_vistaPresetSizer);
		addCustomColorButton(m_windows7PresetSizer);
		m_colorPresetsGroupSizer->Add(m_vistaPresetSizer, 0, wxEXPAND | wxALL, 4);
		m_colorPresetsGroupSizer->Add(m_windows7PresetSizer, 0, wxEXPAND | wxALL, 4);
		sizer->Add(m_colorPresetsGroupSizer, 0, wxEXPAND | wxALL, 4);

		// Keep the frequently used controls visible, matching the original
		// Vista/Windows 7 control-panel flow.
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableTransparency = new wxCheckBox(panel, wxID_ANY, L"启用透明效果");
			row->Add(m_chkEnableTransparency, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::ColorizationOpaqueBlend);
			sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		}
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(
				new wxStaticText(panel, wxID_ANY, L"颜色强度：", wxDefaultPosition, FromDIP(wxSize(150, -1))),
				0,
				wxALIGN_CENTER_VERTICAL | wxRIGHT,
				5
			);
			m_slColorIntensity = new NativeSlider(
				panel,
				wxID_ANY,
				63,
				ColorizationPresets::ClassicIntensityMinimum,
				ColorizationPresets::ClassicIntensityMaximum,
				wxDefaultPosition,
				wxDefaultSize,
				wxSL_HORIZONTAL
			);
			row->Add(m_slColorIntensity, 1, wxALIGN_CENTER_VERTICAL);
			sizer->Add(row, 0, wxEXPAND | wxALL, 8);
		}

		auto* detailedColorizationPane = AddCollapsibleSection(
			panel,
			sizer,
			L"详细颜色设置"
		);
		wxWindow* detailsPanel = detailedColorizationPane->GetPane();
		m_detailedColorizationSizer = new wxBoxSizer(wxVERTICAL);

		// Detailed colorization
		wxBoxSizer* colorsGroup = new wxBoxSizer(wxVERTICAL);
		m_glassColorsGroupSizer = colorsGroup;

		// Horizontal Row for Colors
		wxBoxSizer* colorRow = new wxBoxSizer(wxHORIZONTAL);
		m_colorsRowSizer = colorRow;

		// Active Column
		wxBoxSizer* activeCol = new wxBoxSizer(wxVERTICAL);
		activeCol->Add(new wxStaticText(detailsPanel, wxID_ANY, L"活动"), 0, wxBOTTOM | wxTOP, 5);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpColorizationColor = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			row->Add(m_cpColorizationColor, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationColor, Settings::Id::ColorizationColorOverride);
			activeCol->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(activeCol, 1, wxEXPAND | wxRIGHT, 10);

		// Inactive Column
		m_inactiveColumnSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableInactiveColor = new wxCheckBox(detailsPanel, wxID_ANY, L"自定义非活动颜色");
			m_chkEnableInactiveColor->SetValue(false); // Default logic
			row->Add(m_chkEnableInactiveColor, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationColorInactive);
			m_inactiveColumnSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 5);
		}
		
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpColorizationColorInactive = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			m_cpColorizationColorInactive->Enable(false);
			row->Add(m_cpColorizationColorInactive, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			m_inactiveColumnSizer->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(m_inactiveColumnSizer, 1, wxEXPAND | wxLEFT, 10);

		// Afterglow Column
		m_afterglowColumnSizer = new wxBoxSizer(wxVERTICAL);
		m_afterglowColumnSizer->Add(new wxStaticText(detailsPanel, wxID_ANY, L"Afterglow"), 0, wxBOTTOM | wxTOP, 5);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpAfterglow = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			row->Add(m_cpAfterglow, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationAfterglow, Settings::Id::ColorizationAfterglowOverride);
			m_afterglowColumnSizer->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(m_afterglowColumnSizer, 1, wxEXPAND | wxLEFT, 10);
		
		colorsGroup->Add(colorRow, 0, wxEXPAND | wxALL, 4);

		// Vista-only inactive opacity. Active opacity is the always-visible
		// color-intensity slider above.
		m_vistaOpacitySizer = new wxBoxSizer(wxVERTICAL);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxPanel* labelPanel = new wxPanel(detailsPanel);
			labelPanel->SetMinSize(wxSize(180, -1));
			wxBoxSizer* labelSizer = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableInactiveOpacity = new wxCheckBox(labelPanel, wxID_ANY, wxEmptyString);
			m_chkEnableInactiveOpacity->SetValue(false);
			labelSizer->Add(m_chkEnableInactiveOpacity, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			labelSizer->Add(new wxStaticText(labelPanel, wxID_ANY, L"非活动不透明度："), 0, wxALIGN_CENTER_VERTICAL);
			labelPanel->SetSizer(labelSizer);
			row->Add(labelPanel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			// Spacer to align with the "Auto" dropdown column below
			row->Add(new wxPanel(detailsPanel, wxID_ANY, wxDefaultPosition, wxSize(70, 1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_slGlassOpacityInactive = new NativeSlider(detailsPanel, wxID_ANY, 63, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
			m_slGlassOpacityInactive->Enable(false);
			row->Add(m_slGlassOpacityInactive, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(detailsPanel, row, Settings::Id::GlassOpacityInactive);
			
			m_vistaOpacitySizer->Add(row, 0, wxEXPAND | wxTOP, 2);
		}
		
		colorsGroup->Add(m_vistaOpacitySizer, 0, wxEXPAND | wxALL, 4);
		m_detailedColorizationSizer->Add(colorsGroup, 0, wxEXPAND | wxALL, 4);

		// Win7 Style Parameters
		wxStaticBoxSizer* win7Group = new wxStaticBoxSizer(wxVERTICAL, detailsPanel, L"合成参数");
		m_win7GroupSizer = win7Group; // Assign to member
		
		m_slBlurBalance = new NativeSlider(detailsPanel, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(detailsPanel, win7Group, L"模糊平衡：", m_slBlurBalance, Settings::Id::ColorizationBlurBalance, Settings::Id::ColorizationBlurBalanceOverride);
		
		m_slAfterglowBalance = new NativeSlider(detailsPanel, wxID_ANY, 10, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(detailsPanel, win7Group, L"余辉平衡：", m_slAfterglowBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationAfterglowBalanceOverride);

		m_slColorBalance = new NativeSlider(detailsPanel, wxID_ANY, 10, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
		AddProperty(detailsPanel, win7Group, L"颜色平衡：", m_slColorBalance, Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationColorBalanceOverride);

		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->AddStretchSpacer();
			m_btnPersistCompositionParameters = new wxButton(detailsPanel, wxID_ANY, L"保持当前值");
			m_btnPersistCompositionParameters->SetToolTip(L"将显示的三个合成参数复制为持久的按用户 Override 值。");
			row->Add(m_btnPersistCompositionParameters, 0);
			win7Group->Add(row, 0, wxEXPAND | wxALL, 4);
		}

		m_detailedColorizationSizer->Add(win7Group, 0, wxEXPAND | wxALL, 4);
		detailsPanel->SetSizer(m_detailedColorizationSizer);

		// Advanced colorization
		auto* advancedColorizationPane = AddCollapsibleSection(
			panel,
			sizer,
			L"高级颜色设置"
		);
		wxWindow* advancedPanel = advancedColorizationPane->GetPane();
		auto* advancedSizer = new wxBoxSizer(wxVERTICAL);
		
		auto addBlurBase = [&](wxChoice*& ch, wxColourPickerCtrl*& cp, wxSpinCtrl*& sc, const wxString& label) {
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, label, wxDefaultPosition, wxSize(260, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString modes;
			modes.Add(L"自动");
			modes.Add(L"跟随主题");
			modes.Add(L"自定义");
			ch = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			row->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			cp = new wxColourPickerCtrl(advancedPanel, wxID_ANY);
			cp->Enable(false);
			row->Add(cp, 1, wxALIGN_CENTER_VERTICAL);
			
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, L"Alpha："), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
			sc = new wxSpinCtrl(advancedPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1));
			sc->SetRange(0, 255);
			sc->Enable(false);
			row->Add(sc, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);

			advancedSizer->Add(row, 0, wxEXPAND | wxALL, 4);
		};

		addBlurBase(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, L"基础颜色（透明）：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseTransparent);
		addBlurBase(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, L"基础颜色（最大化）：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseMaximized);
		addBlurBase(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, L"基础颜色（不透明）：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseOpaque);

		wxArrayString priorities;
		priorities.Add(L"Windows Vista");
		priorities.Add(L"Windows 7");
		priorities.Add(L"自动");

		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, L"不透明混合优先级：", wxDefaultPosition, wxSize(260, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			m_chOpaqueBlendPriority = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, priorities);
			row->Add(m_chOpaqueBlendPriority, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(advancedPanel, row, Settings::Id::ColorizationOpaqueBlendPriority);
			
			advancedSizer->Add(row, 0, wxEXPAND | wxALL, 4);
		}
		
		auto addOpacityOverride = [&](wxChoice*& ch, wxSlider*& sl, const wxString& label) {
			wxBoxSizer* r = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* st = new wxStaticText(advancedPanel, wxID_ANY, label, wxDefaultPosition, wxSize(260, -1));
			r->Add(st, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString modes;
			modes.Add(L"自动");
			modes.Add(L"跟随主题");
			modes.Add(L"自定义");
			ch = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxSize(120, -1), modes); // Aligned width
			r->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			sl = new NativeSlider(advancedPanel, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL);
			sl->Enable(false);
			r->Add(sl, 1, wxALIGN_CENTER_VERTICAL);
			
			advancedSizer->Add(r, 0, wxEXPAND | wxALL, 4);
		};

		addOpacityOverride(m_chModeColorizationOpacity, m_slColorizationOpacity, L"基础不透明度：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacity);
		addOpacityOverride(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, L"非活动状态基础不透明度：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityInactive);
		addOpacityOverride(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, L"最大化时基础不透明度：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityMaximized);
		addOpacityOverride(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, L"非活动最大化时基础不透明度：");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityInactiveMaximized);

		advancedPanel->SetSizer(advancedSizer);

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"毛玻璃颜色");
	}
}
