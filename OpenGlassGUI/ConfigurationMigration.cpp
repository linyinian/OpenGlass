#include "pch.h"
#include "ConfigurationMigration.hpp"
#include "RegistryConfig.hpp"
#include "SettingsCatalog.hpp"
#include "ConfigurationMigrationPolicy.hpp"

namespace OpenGlass::ConfigurationMigration
{
	namespace
	{
		using RawValue = std::variant<std::monostate, DWORD, std::wstring>;

		RawValue ReadRaw(const RegistryConfig& config, const Settings::Spec& spec)
		{
			if (spec.type == Settings::ValueType::Dword)
			{
				DWORD value{};
				return config.TryGetDword(std::wstring(spec.name), value) ? RawValue{ value } : RawValue{};
			}
			std::wstring value;
			return config.TryGetString(std::wstring(spec.name), value) ? RawValue{ std::move(value) } : RawValue{};
		}

		HRESULT WriteRaw(RegistryConfig& config, const Settings::Spec& spec, const RawValue& value)
		{
			const std::wstring name(spec.name);
			if (const auto dword = std::get_if<DWORD>(&value))
			{
				return config.SetDword(name, *dword);
			}
			if (const auto string = std::get_if<std::wstring>(&value))
			{
				return config.SetString(name, *string);
			}
			return config.DeleteValue(name);
		}

		HRESULT RestoreAll(
			RegistryConfig& user,
			RegistryConfig& machine,
			const std::array<std::pair<RawValue, RawValue>, Settings::Catalog.size()>& backup
		) noexcept
		{
			HRESULT firstFailure{ S_OK };
			for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
			{
				const auto& spec = Settings::Catalog[index];
				const auto userResult = WriteRaw(user, spec, backup[index].first);
				const auto machineResult = WriteRaw(machine, spec, backup[index].second);
				if (SUCCEEDED(firstFailure) && FAILED(userResult)) firstFailure = userResult;
				if (SUCCEEDED(firstFailure) && FAILED(machineResult)) firstFailure = machineResult;
			}
			return firstFailure;
		}

		bool ConfirmMigration(std::size_t moveCount)
		{
			wxDialog dialog(nullptr, wxID_ANY, L"OpenGlass 配置迁移", wxDefaultPosition, wxSize(680, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* root = new wxBoxSizer(wxVERTICAL);
			const auto message = wxString::Format(
				L"官方 OpenGlass GUI 现在将五个 Windows 颜色值及其 Override 形式按原始交互用户存储，其余由其管理的 OpenGlass 设置则全局存储。\n\n"
				L"OpenGlass 本身仍按先 HKCU 后 HKLM 的顺序读取所有受支持的设置。按用户注册表配置和美化包仍然受支持；此次迁移仅改变官方 GUI 和预设包所管理的存储位置。\n\n"
				L"%zu 个值不在上述推荐位置，需要移动或移除。现有生效值将被保留。选择「退出并稍后迁移」将保持当前配置不变。\n\n"
				L"迁移是事务性的。任何注册表操作失败时，两个配置单元都会被还原，编辑器将不会打开。",
				moveCount
			);
			auto* label = new wxStaticText(&dialog, wxID_ANY, message);
			label->Wrap(570);
			root->Add(label, 1, wxEXPAND | wxALL, 16);
			auto* buttons = new wxBoxSizer(wxHORIZONTAL);
			buttons->AddStretchSpacer();
			auto* exitButton = new wxButton(&dialog, wxID_CANCEL, L"退出并稍后迁移");
			auto* migrateButton = new wxButton(&dialog, wxID_OK, L"迁移并继续");
			buttons->Add(exitButton, 0, wxRIGHT, 8);
			buttons->Add(migrateButton, 0);
			root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
			dialog.SetSizer(root);
			dialog.SetEscapeId(wxID_CANCEL);
			migrateButton->SetDefault();
			return dialog.ShowModal() == wxID_OK;
		}
	}

	bool EnsureCanonicalConfiguration(const std::wstring& userSid)
	{
		RegistryConfig user(RegistryConfig::Mode::User, userSid);
		RegistryConfig machine(RegistryConfig::Mode::Machine, userSid);
		std::array<std::pair<RawValue, RawValue>, Settings::Catalog.size()> backup{};
		std::size_t moveCount{};
		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			backup[index] = { ReadRaw(user, spec), ReadRaw(machine, spec) };
			const bool userPresent = !std::holds_alternative<std::monostate>(backup[index].first);
			const bool machinePresent = !std::holds_alternative<std::monostate>(backup[index].second);
			if ((spec.scope == Settings::Scope::User && machinePresent)
				|| (spec.scope == Settings::Scope::Machine && userPresent))
			{
				++moveCount;
			}
		}

		if (moveCount == 0)
		{
			return true;
		}

		if (!ConfirmMigration(moveCount))
		{
			return false;
		}

		HRESULT failure{ S_OK };
		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			const auto& [userValue, machineValue] = backup[index];
			const auto canonical = ConfigurationMigrationPolicy::Canonicalize(spec.scope, ConfigurationMigrationPolicy::HiveValues<RawValue>{ userValue, machineValue });
			failure = WriteRaw(user, spec, canonical.user);
			if (SUCCEEDED(failure)) failure = WriteRaw(machine, spec, canonical.machine);
			if (FAILED(failure))
			{
				break;
			}
		}
		if (FAILED(failure))
		{
			const auto rollbackFailure = RestoreAll(user, machine, backup);
			wxMessageBox(
				FAILED(rollbackFailure)
					? wxString::Format(L"配置迁移失败 (HRESULT 0x%08lX)，且两个注册表配置单元的还原未完成 (HRESULT 0x%08lX)。", static_cast<unsigned long>(failure), static_cast<unsigned long>(rollbackFailure))
					: wxString::Format(L"配置迁移失败 (HRESULT 0x%08lX)。两个注册表配置单元均已还原。", static_cast<unsigned long>(failure)),
				L"OpenGlass 配置迁移",
				wxOK | wxICON_ERROR
			);
			return false;
		}
		return true;
	}
}
